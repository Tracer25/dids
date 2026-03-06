import os
import re
import math
import fitz
import faiss
import torch
import gradio as gr
import numpy as np
import pytesseract
from PIL import Image
from dataclasses import dataclass
from typing import List, Tuple, Optional, Dict
from sentence_transformers import SentenceTransformer
from transformers import AutoTokenizer, AutoModelForSeq2SeqLM


# -----------------------------
# Config
# -----------------------------
EMBED_MODEL_NAME = os.getenv("EMBED_MODEL_NAME", "sentence-transformers/all-MiniLM-L6-v2")
GEN_MODEL_NAME = os.getenv("GEN_MODEL_NAME", "google/flan-t5-base")
MIN_RETRIEVAL_CONF = float(os.getenv("MIN_RETRIEVAL_CONF", "0.30"))


def _has_tesseract() -> bool:
    try:
        pytesseract.get_tesseract_version()
        return True
    except Exception:
        return False


TESSERACT_AVAILABLE = _has_tesseract()


# -----------------------------
# Model Init
# -----------------------------
embed_model = SentenceTransformer(EMBED_MODEL_NAME)
device = "cuda" if torch.cuda.is_available() else "cpu"


tokenizer = AutoTokenizer.from_pretrained(GEN_MODEL_NAME, use_fast=True)
llm = AutoModelForSeq2SeqLM.from_pretrained(GEN_MODEL_NAME).to(device)
llm.eval()


# -----------------------------
# Data Structures
# -----------------------------
@dataclass
class Chunk:
    text: str
    file_name: str
    page_start: int
    page_end: int
    doc_type: str
    chunk_id: str

    @property
    def source(self) -> str:
        return f"{self.file_name} | Pages {self.page_start}-{self.page_end}"


# -----------------------------
# Document Type Rules
# -----------------------------
def infer_doc_type(filename: str, text_sample: str) -> str:
    name = filename.lower()
    text = text_sample.lower()

    if "mortgage" in name or any(t in text for t in ["interest rate", "escrow", "borrower", "lender", "apr"]):
        return "Mortgage Contract"
    if "fee" in name or any(t in text for t in ["origination fee", "closing cost", "underwriting fee", "points"]):
        return "Lender Fee Sheet"
    if "statement" in name or any(t in text for t in ["beginning balance", "ending balance", "transactions", "debit", "credit"]):
        return "Bank Statement"
    if "invoice" in name or any(t in text for t in ["invoice", "amount due", "bill to", "subtotal"]):
        return "Invoice"
    if "deed" in name or any(t in text for t in ["title", "grantor", "grantee", "legal description"]):
        return "Land Deed"
    return "Other"


# -----------------------------
# Extraction + OCR
# -----------------------------
def _ocr_page(page: fitz.Page) -> str:
    pix = page.get_pixmap(dpi=220)
    img = Image.frombytes("RGB", [pix.width, pix.height], pix.samples)
    return pytesseract.image_to_string(img)


def extract_text_pages(file_obj) -> List[Tuple[int, str]]:
    doc = fitz.open(file_obj.name)
    pages: List[Tuple[int, str]] = []

    for i, page in enumerate(doc):
        text = page.get_text("text")
        text = re.sub(r"\s+", " ", text).strip()

        # If extraction is weak/empty, try OCR (if available).
        if len(text.split()) < 30 and TESSERACT_AVAILABLE:
            try:
                ocr_text = _ocr_page(page)
                ocr_text = re.sub(r"\s+", " ", ocr_text).strip()
                if len(ocr_text) > len(text):
                    text = ocr_text
            except Exception:
                pass

        pages.append((i + 1, text))

    doc.close()
    return pages


# -----------------------------
# Chunking (180 words, 40 overlap)
# -----------------------------
def chunk_pages(file_obj, chunk_words: int = 180, overlap_words: int = 40) -> List[Chunk]:
    pages = extract_text_pages(file_obj)
    file_name = os.path.basename(file_obj.name)

    # Build a token stream carrying page attribution so chunks can span pages without losing source range.
    token_page: List[Tuple[str, int]] = []
    full_sample_parts: List[str] = []
    for page_num, text in pages:
        if not text:
            continue
        words = text.split()
        full_sample_parts.append(" ".join(words[:100]))
        token_page.extend((w, page_num) for w in words)

    if not token_page:
        return []

    doc_type = infer_doc_type(file_name, " ".join(full_sample_parts)[:2000])
    stride = max(1, chunk_words - overlap_words)

    chunks: List[Chunk] = []
    chunk_idx = 0
    for start in range(0, len(token_page), stride):
        end = min(len(token_page), start + chunk_words)
        window = token_page[start:end]
        if not window:
            continue

        chunk_text = " ".join(w for w, _ in window).strip()
        if not chunk_text:
            continue

        page_start = window[0][1]
        page_end = window[-1][1]

        chunks.append(
            Chunk(
                text=chunk_text,
                file_name=file_name,
                page_start=page_start,
                page_end=page_end,
                doc_type=doc_type,
                chunk_id=f"{file_name}_c{chunk_idx}",
            )
        )
        chunk_idx += 1

        if end == len(token_page):
            break

    return chunks


# -----------------------------
# Retriever
# -----------------------------
class Retriever:
    def __init__(self):
        self.index: Optional[faiss.IndexFlatIP] = None
        self.chunks: List[Chunk] = []
        self.by_doc_type: Dict[str, List[int]] = {}

    def build(self, chunks: List[Chunk]):
        self.chunks = chunks
        self.by_doc_type = {}
        self.index = None

        if not chunks:
            return

        texts = [c.text for c in chunks]
        emb = embed_model.encode(
            texts,
            convert_to_numpy=True,
            normalize_embeddings=True,
            batch_size=64,
            show_progress_bar=True,
        ).astype("float32")

        self.index = faiss.IndexFlatIP(emb.shape[1])
        self.index.add(emb)

        for i, c in enumerate(chunks):
            self.by_doc_type.setdefault(c.doc_type, []).append(i)

    @staticmethod
    def _cos_to_conf(cos_score: float) -> float:
        return max(0.0, min(1.0, (cos_score + 1.0) / 2.0))

    def search(self, query: str, k: int = 4, filter_doc_type: Optional[str] = None) -> List[Tuple[Chunk, float]]:
        if self.index is None or not query.strip():
            return []

        q = embed_model.encode([query], convert_to_numpy=True, normalize_embeddings=True).astype("float32")
        overfetch = min(max(k * 6, 16), len(self.chunks))
        D, I = self.index.search(q, overfetch)
        pairs = [(int(i), float(s)) for i, s in zip(I[0], D[0]) if i != -1]

        if filter_doc_type and filter_doc_type != "All":
            allowed = set(self.by_doc_type.get(filter_doc_type, []))
            pairs = [p for p in pairs if p[0] in allowed]

        top = pairs[:k]
        return [(self.chunks[i], self._cos_to_conf(s)) for i, s in top]


retriever = Retriever()


# -----------------------------
# Routing
# -----------------------------
def predict_query_doc_type(query: str) -> Tuple[str, float]:
    q = query.lower()
    rules = {
        "Mortgage Contract": ["mortgage", "interest", "apr", "loan", "escrow", "principal", "payment"],
        "Lender Fee Sheet": ["fee", "closing cost", "origination", "points", "charges", "underwriting"],
        "Bank Statement": ["balance", "deposit", "withdrawal", "transaction", "account"],
        "Invoice": ["invoice", "amount due", "bill", "subtotal", "total"],
        "Land Deed": ["deed", "title", "ownership", "property", "grantor", "grantee"],
    }

    best_type, best_score = "Other", 0
    for doc_type, kws in rules.items():
        score = sum(1 for kw in kws if kw in q)
        if score > best_score:
            best_type, best_score = doc_type, score

    if best_score == 0:
        return "Other", 0.35
    return best_type, min(0.95, 0.50 + 0.12 * best_score)


# -----------------------------
# Prompt + Generation
# -----------------------------
def build_prompt(query: str, retrieved: List[Tuple[Chunk, float]]) -> str:
    context = "\n\n".join(
        f"[{i+1}] {chunk.source} | {chunk.doc_type} | score={score:.2f}\n{chunk.text}"
        for i, (chunk, score) in enumerate(retrieved)
    )

    if not context:
        context = "No relevant content."

    return (
        "You are a mortgage document assistant.\n"
        "You must answer ONLY from the provided context.\n"
        "If the context is insufficient, output exactly: \"I don't have enough information in the provided documents.\"\n"
        "Do not invent facts, numbers, dates, or fees.\n"
        "Answer in 2-5 concise sentences and cite sources as [1], [2] using the context item numbers.\n\n"
        f"Question: {query}\n\n"
        f"Context:\n{context}\n\n"
        "Final answer:"
    )


def generate(prompt: str) -> str:
    inputs = tokenizer(
        prompt,
        return_tensors="pt",
        truncation=True,
        max_length=1024,
    ).to(device)

    with torch.no_grad():
        out = llm.generate(
            **inputs,
            max_new_tokens=220,
            min_new_tokens=30,
            do_sample=False,
            num_beams=4,
            no_repeat_ngram_size=3,
            repetition_penalty=1.1,
            early_stopping=True,
        )

    text = tokenizer.decode(out[0], skip_special_tokens=True).strip()
    text = re.sub(r"\s+", " ", text)

    if re.fullmatch(r"[\d\W]+", text or ""):
        return "I don't have enough information in the provided documents."
    return text


_STOPWORDS = {
    "the", "a", "an", "and", "or", "to", "of", "in", "for", "on", "at", "is", "are", "was", "were",
    "be", "it", "this", "that", "with", "as", "by", "from", "if", "then", "than", "into", "about",
}


def _tokenize_words(text: str) -> List[str]:
    return re.findall(r"[a-zA-Z][a-zA-Z0-9%-]{1,}", text.lower())


def _citation_numbers(text: str) -> List[int]:
    nums = []
    for m in re.findall(r"\[(\d+)\]", text):
        try:
            nums.append(int(m))
        except ValueError:
            continue
    return sorted(set(nums))


def extractive_fallback_answer(query: str, retrieved: List[Tuple[Chunk, float]], max_sentences: int = 3) -> str:
    if not retrieved:
        return "I don't have enough information in the provided documents."

    q_terms = {w for w in _tokenize_words(query) if w not in _STOPWORDS}
    candidates: List[Tuple[float, str, int]] = []

    for idx, (chunk, score) in enumerate(retrieved, start=1):
        sentences = re.split(r"(?<=[.!?])\s+", chunk.text)
        for sent in sentences:
            sent_terms = {w for w in _tokenize_words(sent) if w not in _STOPWORDS}
            overlap = len(q_terms & sent_terms)
            if overlap == 0:
                continue
            lexical = overlap / max(1, len(q_terms))
            combined = 0.65 * lexical + 0.35 * score
            candidates.append((combined, sent.strip(), idx))

    if not candidates:
        top_chunk, _ = retrieved[0]
        preview = " ".join(top_chunk.text.split()[:45]).strip()
        return f"{preview}... [1]"

    candidates.sort(key=lambda x: x[0], reverse=True)
    picked: List[Tuple[str, int]] = []
    used_sentences = set()

    for _, sent, idx in candidates:
        norm = sent.lower()
        if norm in used_sentences:
            continue
        used_sentences.add(norm)
        picked.append((sent, idx))
        if len(picked) >= max_sentences:
            break

    body = " ".join(f"{s} [{i}]" for s, i in picked)
    return body if body else "I don't have enough information in the provided documents."


def validate_and_repair_answer(answer: str, query: str, retrieved: List[Tuple[Chunk, float]]) -> str:
    abstain = "I don't have enough information in the provided documents."
    if not answer.strip():
        return abstain

    if answer.strip() == abstain:
        return answer.strip()

    context_text = " ".join(chunk.text for chunk, _ in retrieved).lower()
    ctx_terms = set(_tokenize_words(context_text))
    ans_terms = [w for w in _tokenize_words(answer) if w not in _STOPWORDS]

    if ans_terms:
        support_ratio = sum(1 for w in ans_terms if w in ctx_terms) / len(ans_terms)
    else:
        support_ratio = 0.0

    # If support is weak, switch to extractive answer from retrieved chunks.
    if support_ratio < 0.45:
        return extractive_fallback_answer(query, retrieved)

    # Ensure citations exist and are valid.
    cits = _citation_numbers(answer)
    max_idx = len(retrieved)
    valid_cits = [c for c in cits if 1 <= c <= max_idx]
    if not valid_cits:
        default_cits = " ".join(f"[{i}]" for i in range(1, min(3, max_idx) + 1))
        answer = f"{answer.strip()} {default_cits}".strip()

    return answer.strip()


# -----------------------------
# App Logic
# -----------------------------
def _normalize_history(history):
    if not history:
        return []
    if isinstance(history[0], dict):
        return history

    normalized = []
    for turn in history:
        if isinstance(turn, (list, tuple)) and len(turn) == 2:
            normalized.append({"role": "user", "content": str(turn[0])})
            normalized.append({"role": "assistant", "content": str(turn[1])})
    return normalized


def index_pdfs(files):
    if not files:
        return "No files uploaded.", 0, gr.update(choices=["All"], value="All")

    all_chunks: List[Chunk] = []
    for f in files:
        try:
            all_chunks.extend(chunk_pages(f, chunk_words=180, overlap_words=40))
        except Exception as e:
            return f"Failed while processing {os.path.basename(f.name)}: {e}", 0, gr.update(choices=["All"], value="All")

    retriever.build(all_chunks)

    if not all_chunks:
        return "No text could be extracted from uploaded PDFs.", 0, gr.update(choices=["All"], value="All")

    doc_types = sorted({c.doc_type for c in all_chunks})
    choices = ["All"] + doc_types

    tesseract_note = "OCR enabled" if TESSERACT_AVAILABLE else "OCR unavailable (install Tesseract for scanned PDFs)"
    return (
        f"Indexed {len(all_chunks)} chunks from {len(files)} file(s). {tesseract_note}.",
        len(all_chunks),
        gr.update(choices=choices, value="All"),
    )


def chat_fn(message, history, k, doc_filter, auto_route):
    history = _normalize_history(history)

    if not message or not message.strip():
        return history, "No sources.", "Chunks used: 0 | Confidence: 0.00 | Route: none"

    if retriever.index is None:
        history += [
            {"role": "user", "content": message},
            {"role": "assistant", "content": "Upload and index PDFs first."},
        ]
        return history, "No sources.", "Chunks used: 0 | Confidence: 0.00 | Route: none"

    active_filter = doc_filter
    route_note = "manual"

    if auto_route and doc_filter == "All":
        pred_type, pred_conf = predict_query_doc_type(message)
        if pred_type != "Other" and pred_conf >= 0.70 and pred_type in retriever.by_doc_type:
            active_filter = pred_type
            route_note = f"auto->{pred_type} ({pred_conf:.2f})"
        else:
            route_note = f"auto->all ({pred_conf:.2f})"

    results = retriever.search(message, int(k), filter_doc_type=active_filter)
    avg_conf = (sum(s for _, s in results) / len(results)) if results else 0.0

    abstain = "I don't have enough information in the provided documents."

    if not results or avg_conf < MIN_RETRIEVAL_CONF:
        answer = abstain
    else:
        try:
            prompt = build_prompt(message, results)
            raw_answer = generate(prompt)
            answer = validate_and_repair_answer(raw_answer, message, results)
        except Exception as e:
            answer = f"Model request failed: {e}"

    sources = "\n".join(
        f"[{i+1}] {c.source} | {c.doc_type} | chunk_id={c.chunk_id} | confidence={s:.2f}"
        for i, (c, s) in enumerate(results)
    ) or "No sources."

    meta = f"Chunks used: {len(results)} | Confidence: {avg_conf:.2f} | Route: {route_note}"

    history += [
        {"role": "user", "content": message},
        {"role": "assistant", "content": answer.strip()},
    ]
    return history, sources, meta


# -----------------------------
# UI
# -----------------------------
with gr.Blocks(title="Mortgage RAG Chatbot") as demo:
    gr.Markdown("## Mortgage Document RAG Chatbot")

    with gr.Row():
        with gr.Column(scale=2):
            chatbot = gr.Chatbot(height=500, type="messages")
            user_input = gr.Textbox(placeholder="Ask about interest rates, escrow, fees, closing costs, payments...", label="Question")

            with gr.Row():
                ask = gr.Button("Ask", variant="primary")
                clear = gr.Button("Clear")

            with gr.Row():
                k_slider = gr.Slider(1, 10, value=4, step=1, label="Top K")
                auto_route = gr.Checkbox(value=True, label="Auto-route by query")

            doc_filter = gr.Dropdown(choices=["All"], value="All", label="Document Filter")
            sources_box = gr.Textbox(label="Sources", lines=8)
            meta_box = gr.Textbox(label="Retrieval Info", lines=1)

        with gr.Column(scale=1):
            pdf_input = gr.File(file_types=[".pdf"], file_count="multiple", label="Upload Mortgage PDFs")
            index_btn = gr.Button("Build Index")
            status = gr.Textbox(label="Status")
            chunk_count = gr.Number(label="Chunks Indexed", precision=0)

    index_btn.click(index_pdfs, pdf_input, [status, chunk_count, doc_filter])

    ask.click(
        chat_fn,
        [user_input, chatbot, k_slider, doc_filter, auto_route],
        [chatbot, sources_box, meta_box],
    ).then(lambda: "", None, user_input)

    clear.click(lambda: ([], "", ""), None, [chatbot, sources_box, meta_box])


if __name__ == "__main__":
    demo.launch(debug=False)

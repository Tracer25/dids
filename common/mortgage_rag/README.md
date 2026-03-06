# Mortgage Document RAG Chatbot

End-to-end local RAG system for mortgage PDFs with OCR fallback, metadata tagging, FAISS retrieval, grounded generation, citations, and Gradio UI.

## Features
- PDF upload and indexing from UI
- Text extraction with OCR fallback for scanned pages (Tesseract)
- Chunking at ~180 words with 40-word overlap
- Metadata tagging (`Mortgage Contract`, `Lender Fee Sheet`, `Bank Statement`, `Invoice`, `Land Deed`, `Other`)
- Sentence-transformer embeddings + FAISS `IndexFlatIP`
- Optional auto-routing/filtering by document type
- Confidence display (derived from cosine similarity)
- Grounded prompt + post-generation validation/fallback to reduce hallucinations

## Setup
1. Create and activate environment.
2. Install Python deps:
   ```bash
   pip install -r requirements.txt
   ```
3. Install Tesseract OCR (required for scanned PDFs):
   - macOS: `brew install tesseract`
   - Ubuntu: `sudo apt-get install tesseract-ocr`

## Run
```bash
python app.py
```

Open the local Gradio URL shown in terminal.

## Notes on Accuracy
- No system can guarantee correct answers for *every* query.
- This app is optimized for grounded behavior by:
  - answering only from retrieved chunks,
  - abstaining when retrieval confidence is low,
  - enforcing citations,
  - using extractive fallback when generated text is weakly supported.

## Tunables
Environment variables:
- `EMBED_MODEL_NAME` (default: `sentence-transformers/all-MiniLM-L6-v2`)
- `GEN_MODEL_NAME` (default: `google/flan-t5-base`)
- `MIN_RETRIEVAL_CONF` (default: `0.30`)

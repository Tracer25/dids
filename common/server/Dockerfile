FROM debian:bookworm-slim

RUN apt-get update && apt-get install -y --no-install-recommends \
    g++ make libc6-dev kcat ca-certificates && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /app/common/server
COPY common /app/common
RUN make

EXPOSE 9000 9464
CMD ["./server", "9000", "10", "9464"]

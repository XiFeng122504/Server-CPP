#!/usr/bin/env python3

import binascii
import os
import pathlib
import signal
import socket
import struct
import subprocess
import sys
import threading
import time

FRAME_START = 1
FRAME_CHUNK = 2
FRAME_END = 3
FRAME_CANCEL = 4


def crc32(data: bytes) -> int:
    return binascii.crc32(data) & 0xFFFFFFFF


def encode_message(frame_type: int, file_id: str, *, payload: bytes = b"", **kwargs) -> bytes:
    body = bytearray()
    file_id_bytes = file_id.encode("utf-8")
    body.append(frame_type)
    body += struct.pack("!H", len(file_id_bytes))
    body += file_id_bytes

    if frame_type == FRAME_START:
        target_path = kwargs["target_path"].encode("utf-8")
        body += struct.pack("!H", len(target_path))
        body += target_path
        body += struct.pack("!Q", kwargs["total_size"])
        body += struct.pack("!I", kwargs["total_chunks"])
        body += struct.pack("!I", kwargs["final_checksum"])
    elif frame_type == FRAME_CHUNK:
        body += struct.pack("!I", kwargs["sequence_number"])
        body += struct.pack("!I", kwargs["checksum"])
        body += struct.pack("!I", len(payload))
        body += payload
    elif frame_type == FRAME_END:
        body += struct.pack("!I", kwargs["final_checksum"])
    elif frame_type == FRAME_CANCEL:
        reason = kwargs.get("reason", "").encode("utf-8")
        body += struct.pack("!H", len(reason))
        body += reason

    return struct.pack("!I", len(body)) + body


def wait_for_port(port: int, timeout: float = 5.0) -> None:
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            with socket.create_connection(("127.0.0.1", port), timeout=0.2):
                return
        except OSError:
            time.sleep(0.1)
    raise RuntimeError(f"Timed out waiting for port {port}")


def run_small_file_test(upload_dir: pathlib.Path) -> None:
    file_id = "small-file"
    data = b"hello upload" * 5
    total_size = len(data)
    checksum = crc32(data)

    sock = socket.create_connection(("127.0.0.1", 8080))
    try:
        start_msg = encode_message(
            FRAME_START,
            file_id,
            target_path="small.bin",
            total_size=total_size,
            total_chunks=1,
            final_checksum=checksum,
        )
        chunk_msg = encode_message(
            FRAME_CHUNK,
            file_id,
            sequence_number=0,
            checksum=crc32(data),
            payload=data,
        )
        end_msg = encode_message(FRAME_END, file_id, final_checksum=checksum)

        sock.sendall(start_msg)
        sock.sendall(chunk_msg)
        sock.sendall(end_msg)
    finally:
        sock.close()

    target_file = upload_dir / "small.bin"
    deadline = time.time() + 2
    while time.time() < deadline and not target_file.exists():
        time.sleep(0.1)

    assert target_file.exists(), "small file was not uploaded"
    assert target_file.read_bytes() == data, "small file content mismatch"


def run_large_file_test(upload_dir: pathlib.Path) -> None:
    file_id = "large-file"
    data = os.urandom(96 * 1024)
    chunk_size = 4096
    chunks = [data[i : i + chunk_size] for i in range(0, len(data), chunk_size)]
    total_size = len(data)
    total_chunks = len(chunks)
    final_checksum = crc32(data)

    sock = socket.create_connection(("127.0.0.1", 8080))
    try:
        start_msg = encode_message(
            FRAME_START,
            file_id,
            target_path="large.bin",
            total_size=total_size,
            total_chunks=total_chunks,
            final_checksum=final_checksum,
        )
        sock.sendall(start_msg)

        order = list(range(total_chunks))
        if total_chunks > 3:
            order[0], order[1] = order[1], order[0]
            order[-1], order[-2] = order[-2], order[-1]

        for seq in order:
            payload = chunks[seq]
            chunk_msg = encode_message(
                FRAME_CHUNK,
                file_id,
                sequence_number=seq,
                checksum=crc32(payload),
                payload=payload,
            )
            sock.sendall(chunk_msg)

        # send a duplicate chunk to simulate retransmission
        dup_msg = encode_message(
            FRAME_CHUNK,
            file_id,
            sequence_number=order[0],
            checksum=crc32(chunks[order[0]]),
            payload=chunks[order[0]],
        )
        sock.sendall(dup_msg)

        end_msg = encode_message(FRAME_END, file_id, final_checksum=final_checksum)
        sock.sendall(end_msg)
    finally:
        sock.close()

    target_file = upload_dir / "large.bin"
    deadline = time.time() + 4
    while time.time() < deadline and not target_file.exists():
        time.sleep(0.1)

    assert target_file.exists(), "large file was not uploaded"
    assert target_file.read_bytes() == data, "large file content mismatch"


def run_interrupted_test(upload_dir: pathlib.Path) -> None:
    file_id = "interrupted-file"
    data = b"partial data that never completes"
    checksum = crc32(data)

    sock = socket.create_connection(("127.0.0.1", 8080))
    start_msg = encode_message(
        FRAME_START,
        file_id,
        target_path="interrupted.bin",
        total_size=len(data) * 2,
        total_chunks=2,
        final_checksum=checksum,
    )
    chunk_msg = encode_message(
        FRAME_CHUNK,
        file_id,
        sequence_number=0,
        checksum=crc32(data),
        payload=data,
    )
    sock.sendall(start_msg)
    sock.sendall(chunk_msg)
    sock.close()

    time.sleep(1.0)

    assert not (upload_dir / "interrupted.bin").exists(), "interrupted file should not exist"
    temp_candidates = list(upload_dir.glob("interrupted.bin.upload"))
    assert not temp_candidates, "temporary file not cleaned after interruption"


def main() -> int:
    repo_root = pathlib.Path(__file__).resolve().parents[1]
    upload_dir = repo_root / "uploads"
    if upload_dir.exists():
        for item in upload_dir.iterdir():
            if item.is_file():
                item.unlink()
    else:
        upload_dir.mkdir(parents=True)

    server_path = repo_root / "build" / "VimUsing"
    if not server_path.exists():
        print("Server binary not found. Build the project before running tests.", file=sys.stderr)
        return 1

    proc = subprocess.Popen([str(server_path)], cwd=repo_root, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    stdout_lines: list[str] = []
    stderr_lines: list[str] = []

    def _reader(stream, sink):
        with stream:
            for line in iter(stream.readline, b""):
                sink.append(line.decode(errors="ignore"))

    stdout_thread = threading.Thread(target=_reader, args=(proc.stdout, stdout_lines), daemon=True)
    stderr_thread = threading.Thread(target=_reader, args=(proc.stderr, stderr_lines), daemon=True)
    stdout_thread.start()
    stderr_thread.start()
    result = 0
    try:
        wait_for_port(8080)

        run_small_file_test(upload_dir)
        run_large_file_test(upload_dir)
        run_interrupted_test(upload_dir)

        print("All file upload tests passed")
    finally:
        if stdout_lines:
            print("Server stdout:\n" + "".join(stdout_lines))
        if stderr_lines:
            print("Server stderr:\n" + "".join(stderr_lines), file=sys.stderr)
        if proc.poll() is None:
            proc.send_signal(signal.SIGINT)
            try:
                proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                proc.kill()
    return result


if __name__ == "__main__":
    sys.exit(main())

# Plivo Assignment Design Notes

This architecture relies on a low-latency hybrid mechanism combining proactive FEC with reactive ARQ:
1. **Forward Error Correction (FEC):** The sender produces an XOR-computed redundancy packet for every block of two packets (pairing 0 and 1, 2 and 3, etc.) to handle single-packet drops with zero delay.
2. **NACK Fallback (ARQ):** For deep burst drops that destroy both raw payloads and the parity block, the receiver queries the sender via port 47003 for precise sequence retransmissions.
3. **Recommended Grading Delay:** We recommend grading at **120ms** playout delay. This ensures both Profile A and Profile B achieve a VALID status with near-zero deadline misses under heavy network jitter.
4. **What breaks it:** The system performance degrades if a raw network blackout duration exceeds our playout window buffer size (120ms), or if the round-trip time for a NACK request-reply exceeds this window.

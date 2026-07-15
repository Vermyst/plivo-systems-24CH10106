# Plivo Assignment Design Notes

This architecture relies on a low-latency hybrid mechanism combining proactive FEC with reactive ARQ:
1. **Forward Error Correction:** The sender produces an XOR-computed redundancy packet for every block of two packets (pairing 0 and 1, 2 and 3, etc.).
2. **NACK Fallback:** For deep burst drops that kill both raw payloads and the parity block, the receiver queries the sender for precise sequence retransmissions if missing for over 40ms.
3. We recommend grading at **80ms** playout delay.
4. The system performance breaks if a raw network blackout duration exceeds our playout window buffer size (80ms).
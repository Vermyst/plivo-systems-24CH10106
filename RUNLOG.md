# Plivo Assignment Run Log

## Profile A Execution
* **Configuration:** `--profile profiles/A.json --delay_ms 80`
* **Result:** VALID
* **Miss Rate:** 0.00%
* **Overhead:** 1.55x
* **Strategy details:** Pairwise XOR FEC recovers single packet drops instantly without waiting for retransmissions.

## Profile B Execution
* **Configuration:** `--profile profiles/B.json --delay_ms 120`
* **Result:** VALID
* **Miss Rate:** 0.40%
* **Overhead:** 1.73x
* **Strategy details:** Combined pairwise XOR FEC for immediate single packet losses, backed by tight NACK feedback targeting sequence gaps. Increased playout delay to 120ms to allow retransmitted packets sufficient round-trip time to arrive before their playout deadlines.

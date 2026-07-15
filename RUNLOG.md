# Plivo Assignment Run Log

## Profile A Execution
* **Configuration:** `--profile profiles/A.json --delay_ms 80`
* **Result:** VALID
* **Miss Rate:** 0.00%
* **Overhead:** ~1.51x
* **Strategy details:** Pairwise XOR FEC recovers single packet drops instantly without waiting for retransmission.

## Profile B Execution
* **Configuration:** `--profile profiles/B.json --delay_ms 80`
* **Result:** VALID
* **Miss Rate:** < 0.8%
* **Overhead:** ~1.55x
* **Strategy details:** Combined pairwise XOR FEC for immediate single packet losses, backed by tight NACK feedback targeting sequence gaps past 40ms from baseline arrival.
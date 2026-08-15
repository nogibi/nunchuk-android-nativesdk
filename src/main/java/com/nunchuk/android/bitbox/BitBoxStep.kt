package com.nunchuk.android.bitbox

data class BitBoxStep(
    val type: BitBoxStepType,
    val interaction: BitBoxUserInteraction = BitBoxUserInteraction.NONE,
    val writes: List<ByteArray> = emptyList(),
    val retryAfterMs: Long = 0,
    val pairingCode: String? = null,
    val error: BitBoxError? = null,
    val progress: Double = 0.0,
)

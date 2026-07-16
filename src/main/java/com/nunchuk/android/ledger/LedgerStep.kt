package com.nunchuk.android.ledger

data class LedgerStep(
    val type: LedgerStepType,
    val interaction: UserInteraction = UserInteraction.NONE,
    val writes: List<ByteArray> = emptyList(),
    val error: LedgerError? = null,
)

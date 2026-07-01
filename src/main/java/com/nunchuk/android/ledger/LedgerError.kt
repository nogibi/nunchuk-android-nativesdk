package com.nunchuk.android.ledger

data class LedgerError(
    val code: Int,
    val message: String,
    val statusWord: Int,
)

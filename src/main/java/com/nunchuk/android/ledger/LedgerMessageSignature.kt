package com.nunchuk.android.ledger

data class LedgerMessageSignature(
    val v: Int,
    val r: ByteArray,
    val s: ByteArray,
)

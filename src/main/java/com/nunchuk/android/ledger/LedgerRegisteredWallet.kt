package com.nunchuk.android.ledger

data class LedgerRegisteredWallet(
    val name: String,
    val descriptorTemplate: String,
    val keysInfo: List<String>,
    val hmac: ByteArray,
)

package com.nunchuk.android.ledger

data class LedgerWalletPolicy(
    val name: String,
    val descriptorTemplate: String,
    val keysInfo: List<String>,
)

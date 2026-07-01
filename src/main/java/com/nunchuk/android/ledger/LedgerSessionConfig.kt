package com.nunchuk.android.ledger

data class LedgerSessionConfig(
    val transport: LedgerTransport,
    val packetSize: Int,
    val channel: Int = 0x0101,
)

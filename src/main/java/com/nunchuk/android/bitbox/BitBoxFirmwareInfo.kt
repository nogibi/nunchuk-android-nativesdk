package com.nunchuk.android.bitbox

data class BitBoxFirmwareInfo(
    val product: BitBoxProduct,
    val monotonicVersion: Long,
    val firmwareSize: Long,
)

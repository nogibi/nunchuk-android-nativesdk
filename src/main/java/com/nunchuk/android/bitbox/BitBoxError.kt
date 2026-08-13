package com.nunchuk.android.bitbox

data class BitBoxError(
    val code: Int,
    val message: String,
    val deviceCode: Int,
)

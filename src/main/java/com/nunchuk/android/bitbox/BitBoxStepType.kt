package com.nunchuk.android.bitbox

enum class BitBoxStepType {
    WRITE,
    READ_MORE,
    RETRY_AFTER,
    AWAITING_USER,
    COMPLETE,
    FAILED,
}

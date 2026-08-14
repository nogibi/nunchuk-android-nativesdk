package com.nunchuk.android.bitbox

/** Recovery-word length for [BitBoxNativeClient.createNewSeed]. */
enum class BitBoxMnemonicLength {
    /** Requires BitBox firmware 9.6.0 or newer. */
    WORDS_12,
    WORDS_24,
}

/** Metadata returned by [BitBoxNativeClient.getBackupsResult]. */
data class BitBoxBackup(
    /** Firmware backup identifier passed unchanged to `restoreBackup`. */
    val id: String,
    val name: String,
    /** Backup creation time as Unix seconds. */
    val timestamp: Long,
)

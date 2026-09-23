package com.nunchuk.android.satochip

import androidx.annotation.Keep

// Callbacks run synchronously on the caller's thread in one authenticated card session.
@Keep
interface SatochipCard {
    fun getExtendedKey(path: String): Array<ByteArray>
    fun importSeed(seed: ByteArray) { throw UnsupportedOperationException("Seed import unavailable") }
    fun signTransactionHash(keyNumber: Int, hash: ByteArray): ByteArray
    fun tweakPrivateKey(keyNumber: Int, tweak: ByteArray, bypass: Boolean): ByteArray
    fun signSchnorrHash(hash: ByteArray): ByteArray
    fun generateNonce(keyNumber: Int, aggregateKey: ByteArray, hash: ByteArray, extra: ByteArray): Array<ByteArray>
    fun signMusig2(keyNumber: Int, nonce: ByteArray, b: ByteArray, ea: ByteArray, evenY: Boolean, positiveKey: Boolean): ByteArray
    fun onProgress(percent: Int): Boolean = true
}

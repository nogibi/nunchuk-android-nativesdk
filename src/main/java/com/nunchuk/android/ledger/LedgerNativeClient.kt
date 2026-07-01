package com.nunchuk.android.ledger

import com.nunchuk.android.exception.NCNativeException
import com.nunchuk.android.nativelib.LibNunchukAndroid

class LedgerNativeClient {
    private val native = LibNunchukAndroid()

    @Throws(NCNativeException::class)
    fun createSession(sessionId: String, config: LedgerSessionConfig) {
        native.ledgerCreateSession(sessionId, config)
    }

    @Throws(NCNativeException::class)
    fun reset(sessionId: String) {
        native.ledgerReset(sessionId)
    }

    @Throws(NCNativeException::class)
    fun getExtendedPublicKey(
        sessionId: String,
        derivationPath: String,
        checkOnDevice: Boolean = false,
    ): LedgerStep = native.ledgerGetExtendedPublicKey(
        sessionId = sessionId,
        derivationPath = derivationPath,
        checkOnDevice = checkOnDevice,
    )

    @Throws(NCNativeException::class)
    fun onData(sessionId: String, data: ByteArray): LedgerStep {
        return native.ledgerOnData(sessionId, data)
    }

    @Throws(NCNativeException::class)
    fun getExtendedPublicKeyResult(sessionId: String): String {
        return native.ledgerGetExtendedPublicKeyResult(sessionId)
    }
}

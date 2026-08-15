package com.nunchuk.android.bitbox

import com.nunchuk.android.exception.NCNativeException
import com.nunchuk.android.model.SingleSigner
import com.nunchuk.android.model.Wallet
import com.nunchuk.android.model.bridge.toBridge
import com.nunchuk.android.nativelib.LibNunchukAndroid

/** Requires [com.nunchuk.android.nativelib.NunchukNativeSdk.initNunchuk] first. */
class BitBoxNativeClient {
    private val native = LibNunchukAndroid()

    @Throws(NCNativeException::class)
    fun createSession(
        sessionId: String,
        transport: BitBoxTransport,
    ): BitBoxStep = native.bitBoxCreateSession(sessionId, transport)

    @Throws(NCNativeException::class)
    fun confirmPairing(sessionId: String, accepted: Boolean): BitBoxStep =
        native.bitBoxConfirmPairing(sessionId, accepted)

    @Throws(NCNativeException::class)
    fun setDeviceName(sessionId: String, name: String): BitBoxStep =
        native.bitBoxSetDeviceName(sessionId, name)

    @Throws(NCNativeException::class)
    fun changePassword(sessionId: String): BitBoxStep = native.bitBoxChangePassword(sessionId)

    @Throws(NCNativeException::class)
    fun setMnemonicPassphraseEnabled(sessionId: String, enabled: Boolean): BitBoxStep =
        native.bitBoxSetMnemonicPassphraseEnabled(sessionId, enabled)

    @Throws(NCNativeException::class)
    fun factoryReset(sessionId: String): BitBoxStep = native.bitBoxFactoryReset(sessionId)

    @Throws(NCNativeException::class)
    fun inspectFirmware(firmware: ByteArray): BitBoxFirmwareInfo =
        native.bitBoxInspectFirmware(firmware)

    @Throws(NCNativeException::class)
    fun enterFirmwareUpgrade(sessionId: String): BitBoxStep =
        native.bitBoxEnterFirmwareUpgrade(sessionId)

    @Throws(NCNativeException::class)
    fun startFirmwareUpgrade(
        sessionId: String,
        product: BitBoxProduct,
        firmware: ByteArray,
    ): BitBoxStep = native.bitBoxStartFirmwareUpgrade(sessionId, product, firmware)

    @Throws(NCNativeException::class)
    fun rebootBootloader(sessionId: String, product: BitBoxProduct): BitBoxStep =
        native.bitBoxRebootBootloader(sessionId, product)

    @Throws(NCNativeException::class)
    fun onBootloaderData(sessionId: String, data: ByteArray): BitBoxStep =
        native.bitBoxOnBootloaderData(sessionId, data)

    @Throws(NCNativeException::class)
    fun createNewSeed(
        sessionId: String,
        mnemonicLength: BitBoxMnemonicLength = BitBoxMnemonicLength.WORDS_24,
    ): BitBoxStep = native.bitBoxCreateNewSeed(sessionId, mnemonicLength)

    @Throws(NCNativeException::class)
    fun showMnemonic(sessionId: String): BitBoxStep = native.bitBoxShowMnemonic(sessionId)

    @Throws(NCNativeException::class)
    fun checkSdCard(sessionId: String): BitBoxStep = native.bitBoxCheckSdCard(sessionId)

    @Throws(NCNativeException::class)
    fun insertSdCard(sessionId: String): BitBoxStep = native.bitBoxInsertSdCard(sessionId)

    @Throws(NCNativeException::class)
    fun createBackup(sessionId: String): BitBoxStep = native.bitBoxCreateBackup(sessionId)

    @Throws(NCNativeException::class)
    fun checkBackup(sessionId: String, silent: Boolean = false): BitBoxStep =
        native.bitBoxCheckBackup(sessionId, silent)

    @Throws(NCNativeException::class)
    fun listBackups(sessionId: String): BitBoxStep = native.bitBoxListBackups(sessionId)

    @Throws(NCNativeException::class)
    fun restoreBackup(sessionId: String, id: String): BitBoxStep =
        native.bitBoxRestoreBackup(sessionId, id)

    @Throws(NCNativeException::class)
    fun restoreFromMnemonic(sessionId: String): BitBoxStep =
        native.bitBoxRestoreFromMnemonic(sessionId)

    @Throws(NCNativeException::class)
    fun getExtendedPublicKey(
        sessionId: String,
        derivationPath: String,
        checkOnDevice: Boolean = false,
    ): BitBoxStep = native.bitBoxGetExtendedPublicKey(
        sessionId,
        derivationPath,
        checkOnDevice,
    )

    @Throws(NCNativeException::class)
    fun getMasterFingerprint(sessionId: String): BitBoxStep =
        native.bitBoxGetMasterFingerprint(sessionId)

    @Throws(NCNativeException::class)
    fun isWalletRegistered(sessionId: String, wallet: Wallet): BitBoxStep =
        native.bitBoxIsWalletRegistered(sessionId, wallet.toBridge())

    @Throws(NCNativeException::class)
    fun registerWallet(sessionId: String, wallet: Wallet): BitBoxStep =
        native.bitBoxRegisterWallet(sessionId, wallet.toBridge())

    @Throws(NCNativeException::class)
    fun registerWallet(
        sessionId: String,
        walletContent: String,
        walletName: String,
    ): BitBoxStep = native.bitBoxRegisterWalletContent(
        sessionId,
        walletContent,
        walletName,
    )

    @Throws(NCNativeException::class)
    fun getWalletAddress(
        sessionId: String,
        wallet: Wallet,
        addressIndex: Int,
        checkOnDevice: Boolean = true,
        change: Boolean = false,
    ): BitBoxStep = native.bitBoxGetWalletAddress(
        sessionId,
        wallet.toBridge(),
        addressIndex,
        checkOnDevice,
        change,
    )

    @Throws(NCNativeException::class)
    fun getWalletAddress(
        sessionId: String,
        walletContent: String,
        walletName: String,
        addressIndex: Int,
        checkOnDevice: Boolean = true,
        change: Boolean = false,
    ): BitBoxStep = native.bitBoxGetWalletAddressContent(
        sessionId,
        walletContent,
        walletName,
        addressIndex,
        checkOnDevice,
        change,
    )

    @Throws(NCNativeException::class)
    fun getSignMessagePath(signer: SingleSigner): String = native.bitBoxGetSignMessagePath(signer)

    @Throws(NCNativeException::class)
    fun getSignMessageAddress(signer: SingleSigner): String =
        native.bitBoxGetSignMessageAddress(signer)

    @Throws(NCNativeException::class)
    fun signMessage(
        sessionId: String,
        derivationPath: String,
        message: String,
    ): BitBoxStep = native.bitBoxSignMessage(sessionId, derivationPath, message)

    @Throws(NCNativeException::class)
    fun signPsbt(sessionId: String, wallet: Wallet, psbt: String): BitBoxStep =
        native.bitBoxSignPsbt(sessionId, wallet.toBridge(), psbt)

    @Throws(NCNativeException::class)
    fun signPsbt(
        sessionId: String,
        walletContent: String,
        walletName: String,
        psbt: String,
    ): BitBoxStep = native.bitBoxSignPsbtContent(
        sessionId,
        walletContent,
        walletName,
        psbt,
    )

    @Throws(NCNativeException::class)
    fun resume(sessionId: String): BitBoxStep = native.bitBoxResume(sessionId)

    @Throws(NCNativeException::class)
    fun onData(sessionId: String, data: ByteArray): BitBoxStep =
        native.bitBoxOnData(sessionId, data)

    @Throws(NCNativeException::class)
    fun isInitialized(sessionId: String): Boolean = native.bitBoxIsInitialized(sessionId)

    @Throws(NCNativeException::class)
    fun getDeviceInfo(sessionId: String): BitBoxDeviceInfo =
        native.bitBoxGetDeviceInfo(sessionId)

    @Throws(NCNativeException::class)
    fun getInitializeResult(sessionId: String): BitBoxInitializeResult =
        native.bitBoxGetInitializeResult(sessionId)

    @Throws(NCNativeException::class)
    fun getSdCardInsertedResult(sessionId: String): Boolean =
        native.bitBoxGetSdCardInsertedResult(sessionId)

    @Throws(NCNativeException::class)
    fun getBackupsResult(sessionId: String): List<BitBoxBackup> =
        native.bitBoxGetBackupsResult(sessionId)

    @Throws(NCNativeException::class)
    fun getCheckedBackupIdResult(sessionId: String): String =
        native.bitBoxGetCheckedBackupIdResult(sessionId)

    @Throws(NCNativeException::class)
    fun getExtendedPublicKeyResult(sessionId: String): String =
        native.bitBoxGetExtendedPublicKeyResult(sessionId)

    @Throws(NCNativeException::class)
    fun getMasterFingerprintResult(sessionId: String): String =
        native.bitBoxGetMasterFingerprintResult(sessionId)

    @Throws(NCNativeException::class)
    fun getRegistrationResult(sessionId: String): Boolean =
        native.bitBoxGetRegistrationResult(sessionId)

    @Throws(NCNativeException::class)
    fun getWalletAddressResult(sessionId: String): String =
        native.bitBoxGetWalletAddressResult(sessionId)

    @Throws(NCNativeException::class)
    fun getMessageSignatureResult(sessionId: String): String =
        native.bitBoxGetMessageSignatureResult(sessionId)

    @Throws(NCNativeException::class)
    fun getSignPsbtResult(sessionId: String): String =
        native.bitBoxGetSignPsbtResult(sessionId)
}

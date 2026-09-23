package com.nunchuk.android.satochip

import com.nunchuk.android.model.MasterSigner
import com.nunchuk.android.model.SingleSigner
import com.nunchuk.android.model.Wallet
import com.nunchuk.android.model.bridge.toBridge
import com.nunchuk.android.nativelib.LibNunchukAndroid
import java.text.Normalizer

// Initialize NunchukNativeSdk first. Serialize card operations off the UI thread.
class SatochipNativeClient {
    private val native = LibNunchukAndroid()

    fun getMasterFingerprint(card: SatochipCard): String = native.satochipGetMasterFingerprint(card)

    fun addMasterSigner(fingerprint: String, name: String) =
        native.satochipAddMasterSigner(fingerprint, name)

    fun createMasterSigner(card: SatochipCard, name: String): MasterSigner =
        native.satochipCreateMasterSigner(card, name)

    fun cacheMasterSignerXpubs(card: SatochipCard, masterSignerId: String) =
        native.satochipCacheMasterSignerXpubs(card, masterSignerId)

    fun getSigner(card: SatochipCard, masterSignerId: String, path: String): SingleSigner =
        native.satochipGetSigner(card, masterSignerId, path)

    fun importSeed(card: SatochipCard, mnemonic: String, passphrase: String = "") {
        val words = Normalizer.normalize(mnemonic.trim(), Normalizer.Form.NFKD).toByteArray(Charsets.UTF_8)
        val password = Normalizer.normalize(passphrase, Normalizer.Form.NFKD).toByteArray(Charsets.UTF_8)
        try {
            native.satochipImportSeed(card, words, password)
        } finally {
            words.fill(0)
            password.fill(0)
        }
    }

    fun signPsbt(card: SatochipCard, wallet: Wallet, psbt: String): String =
        native.satochipSignPsbt(card, wallet.toBridge(), psbt)

    fun signPsbt(card: SatochipCard, walletContent: String, walletName: String, psbt: String): String =
        native.satochipSignPsbtContent(card, walletContent, walletName, psbt)
}

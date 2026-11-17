package com.nunchuk.android.onekey

// Keep enum order in sync with libnunchuk's utils/onekey/types.hpp.
enum class OneKeyTransport { USB, BLE }
enum class OneKeyStepType { WRITE, READ_MORE, AWAITING_USER, COMPLETE, FAILED }
enum class OneKeyUserInteraction {
    NONE, UNLOCK_DEVICE, ENTER_PIN, ENTER_PASSPHRASE, VERIFY_ADDRESS,
    EXPORT_PUBLIC_KEY, SIGN_MESSAGE, SIGN_TRANSACTION,
}
enum class OneKeyModel { UNKNOWN, CLASSIC, CLASSIC1S, PRO }

data class OneKeyError(val code: Int, val message: String, val deviceCode: Int)

data class OneKeyStep(
    val type: OneKeyStepType,
    val interaction: OneKeyUserInteraction = OneKeyUserInteraction.NONE,
    val writes: List<ByteArray> = emptyList(),
    val error: OneKeyError? = null,
    val requestCode: Int = 0,
    val onDeviceAvailable: Boolean = false,
    val onDeviceRequired: Boolean = false,
    val attachPinAvailable: Boolean = false,
)

data class OneKeyDeviceInfo(
    val model: OneKeyModel,
    val firmwareVersion: String,
    val deviceId: String,
    val label: String,
    val initialized: Boolean,
    val unlocked: Boolean,
    val pinProtection: Boolean,
    val passphraseProtection: Boolean,
    val passphraseOnDevice: Boolean,
    val bootloader: Boolean,
    val supportsRawPsbt: Boolean,
)

package com.nunchuk.android.jade

enum class JadeStepType {
    WRITE,
    READ_MORE,
    CUSTOM_SERVER_APPROVAL,
    COMPLETE,
    FAILED,
}

enum class JadeUserInteraction {
    NONE,
    SETUP_DEVICE,
    ENTER_PIN,
    VERIFY_ADDRESS,
    REGISTER_WALLET,
    SIGN_MESSAGE,
    SIGN_TRANSACTION,
    APPROVE_PINSERVER,
}

data class JadeError(
    val code: Int,
    val message: String,
    val deviceCode: Int,
    val deviceData: String,
)

data class JadeCustomPinServerInfo(
    val urls: List<String>,
    val method: String,
    val host: String,
)

data class JadeStep(
    val type: JadeStepType,
    val interaction: JadeUserInteraction = JadeUserInteraction.NONE,
    val writes: List<ByteArray> = emptyList(),
    val customServer: JadeCustomPinServerInfo? = null,
    val error: JadeError? = null,
)

enum class JadeDeviceState {
    UNKNOWN,
    LOCKED,
    UNSAVED,
    UNINITIALIZED,
    TEMPORARY,
    READY,
}

enum class JadeDeviceNetworks {
    UNKNOWN,
    MAIN,
    TEST,
    ALL,
}

data class JadeDeviceInfo(
    val firmwareVersion: String,
    val otaMaxChunk: Long,
    val config: String,
    val boardType: String,
    val features: String,
    val idfVersion: String,
    val chipFeatures: String,
    val efuseMac: String?,
    val batteryStatus: Long?,
    val state: JadeDeviceState,
    val networks: JadeDeviceNetworks,
    val hasPin: Boolean,
    val hasPinReported: Boolean,
    val firmwareUpgradeRequired: Boolean,
)

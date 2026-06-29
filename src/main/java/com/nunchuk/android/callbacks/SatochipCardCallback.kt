/**************************************************************************
 * This file is part of the Nunchuk software (https://nunchuk.io/)        *							          *
 * Copyright (C) 2022 Nunchuk								              *
 *                                                                        *
 * This program is free software; you can redistribute it and/or          *
 * modify it under the terms of the GNU General Public License            *
 * as published by the Free Software Foundation; either version 3         *
 * of the License, or (at your option) any later version.                 *
 *                                                                        *
 * This program is distributed in the hope that it will be useful,        *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
 * GNU General Public License for more details.                           *
 *                                                                        *
 * You should have received a copy of the GNU General Public License      *
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.  *
 *                                                                        *
 **************************************************************************/

package com.nunchuk.android.callbacks

import androidx.annotation.Keep

@Keep
abstract class SatochipCardCallback {
    abstract fun cardBip32GetExtendedKey(path: String): Array<ByteArray>

    abstract fun cardSignTransactionHash(
        keyNumber: Int,
        txHash: ByteArray,
        challengeResponse: ByteArray?,
    ): ByteArray

    open fun cardTaprootTweakPrivateKey(
        keyNumber: Int,
        tweak: ByteArray,
        bypassFlag: Boolean,
    ): ByteArray = unsupported("Taproot private key tweaking")

    open fun cardSignSchnorrHash(
        txHash: ByteArray,
        challengeResponse: ByteArray?,
    ): ByteArray = unsupported("Schnorr signing")

    open fun cardMusig2GenerateNonce(
        keyNumber: Int,
        aggregatePublicKey: ByteArray,
        message: ByteArray,
        extra: ByteArray,
    ): Array<ByteArray> = unsupported("MuSig2 nonce generation")

    open fun cardMusig2Sign(
        keyNumber: Int,
        secNonce: ByteArray,
        b: ByteArray,
        ea: ByteArray,
        rHasEvenY: Boolean,
        ggaccIsOne: Boolean,
    ): ByteArray = unsupported("MuSig2 signing")

    private fun unsupported(operation: String): Nothing =
        throw UnsupportedOperationException("$operation is not supported by this Satochip callback")
}

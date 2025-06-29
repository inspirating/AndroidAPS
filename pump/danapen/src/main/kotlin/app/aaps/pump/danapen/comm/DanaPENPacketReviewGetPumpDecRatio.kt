package app.aaps.pump.danapen.comm

import app.aaps.core.interfaces.logging.LTag
import app.aaps.pump.dana.DanaPump
import dagger.android.HasAndroidInjector
import info.nightscout.androidaps.danapen.encryption.BleEncryption
import javax.inject.Inject

class DanaPENPacketReviewGetPumpDecRatio(
    injector: HasAndroidInjector
) : DanaPENPacket(injector) {

    @Inject lateinit var danaPump: DanaPump

    init {
        opCode = BleEncryption.DANAR_PACKET__OPCODE_REVIEW__GET_PUMP_DEC_RATIO
        aapsLogger.debug(LTag.PUMPCOMM, "New message")
    }

    override fun handleMessage(data: ByteArray) {
        danaPump.decRatio = intFromBuff(data, 0, 1) * 5
        failed = false
        aapsLogger.debug(LTag.PUMPCOMM, "Dec ratio: ${danaPump.decRatio}%")
    }

    override val friendlyName: String = "REVIEW__GET_PUMP_DEC_RATIO"
}
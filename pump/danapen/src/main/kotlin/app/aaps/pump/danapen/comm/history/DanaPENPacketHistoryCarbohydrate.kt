package app.aaps.pump.danapen.comm.history

import app.aaps.core.interfaces.logging.LTag
import dagger.android.HasAndroidInjector
import info.nightscout.androidaps.danapen.encryption.BleEncryption

class DanaPENPacketHistoryCarbohydrate(
    injector: HasAndroidInjector,
    from: Long = 0
) : DanaPENPacketHistory(injector, from) {

    init {
        opCode = BleEncryption.DANAR_PACKET__OPCODE_REVIEW__CARBOHYDRATE
        aapsLogger.debug(LTag.PUMPCOMM, "New message")
    }

    override val friendlyName: String = "REVIEW__CARBOHYDRATE"
}
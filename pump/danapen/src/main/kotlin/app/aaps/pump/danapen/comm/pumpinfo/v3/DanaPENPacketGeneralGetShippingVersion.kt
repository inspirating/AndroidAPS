package app.aaps.pump.danapen.comm.pumpinfo.v3

import app.aaps.core.interfaces.logging.LTag
import app.aaps.pump.dana.DanaPump
import app.aaps.pump.danapen.comm.DanaPENPacket
import dagger.android.HasAndroidInjector
import info.nightscout.androidaps.danapen.encryption.BleEncryption
import java.nio.charset.Charset
import javax.inject.Inject

class DanaPENPacketGeneralGetShippingVersion(
    injector: HasAndroidInjector
) : DanaPENPacket(injector) {

    @Inject lateinit var danaPump: DanaPump

    init {
        opCode = BleEncryption.DANAR_PACKET__OPCODE_GENERAL__GET_SHIPPING_VERSION
        aapsLogger.debug(LTag.PUMPCOMM, "New message")
    }

    override fun handleMessage(data: ByteArray) {
        danaPump.bleModel = data.copyOfRange(DATA_START, data.size).toString(Charset.forName("US-ASCII"))
        failed = false
        aapsLogger.debug(LTag.PUMPCOMM, "BLE Model: " + danaPump.bleModel)
    }

    override val friendlyName: String = "GENERAL__GET_SHIPPING_VERSION"
}
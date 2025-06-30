package app.aaps.pump.danapen.comm

import app.aaps.pump.danapen.DanaPENTestBase
import app.aaps.pump.danapen.comm.pumpinfo.DanaPENPacketGeneralGetPumpCheck
import dagger.android.AndroidInjector
import dagger.android.HasAndroidInjector
import org.junit.jupiter.api.Assertions
import org.junit.jupiter.api.Test

class DanaPenPacketGeneralGetPumpCheckTest : DanaPENTestBase() {

    private val packetInjector = HasAndroidInjector {
        AndroidInjector {
            if (it is DanaPENPacketGeneralGetPumpCheck) {
                it.aapsLogger = aapsLogger
                it.rxBus = rxBus
                it.rh = rh
                it.danaPump = danaPump
                it.uiInteraction = uiInteraction
            }
        }
    }

    @Test fun runTest() {
        var packet = DanaPENPacketGeneralGetPumpCheck(packetInjector)
        Assertions.assertEquals(0, packet.getRequestParams().size)
        // test message decoding
        // test for the length message
        packet.handleMessage(createArray(1, 0.toByte()))
        Assertions.assertEquals(true, packet.failed)
        packet = DanaPENPacketGeneralGetPumpCheck(packetInjector)
        packet.handleMessage(createArray(15, 0.toByte()))
        Assertions.assertEquals(false, packet.failed)
        Assertions.assertEquals("REVIEW__GET_PUMP_CHECK", packet.friendlyName)
    }
}
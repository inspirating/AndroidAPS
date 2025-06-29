package app.aaps.pump.danapen.comm

import app.aaps.pump.danapen.DanaPENTestBase
import dagger.android.AndroidInjector
import dagger.android.HasAndroidInjector
import org.junit.jupiter.api.Assertions
import org.junit.jupiter.api.Test

class DanaPENPacketOptionSetUserOptionTest : DanaPENTestBase() {

    private val packetInjector = HasAndroidInjector {
        AndroidInjector {
            if (it is DanaPENPacketOptionSetUserOption) {
                it.aapsLogger = aapsLogger
                it.danaPump = danaPump
            }
        }
    }

    @Test fun runTest() {
        val packet = DanaPENPacketOptionSetUserOption(packetInjector)
        // test params
        val params = packet.getRequestParams()
        Assertions.assertEquals((danaPump.lcdOnTimeSec and 0xff).toByte(), params[3])
        // test message decoding
        packet.handleMessage(createArray(3, 0.toByte()))
        Assertions.assertEquals(false, packet.failed)
        // everything ok :)
        packet.handleMessage(createArray(17, 1.toByte()))
        Assertions.assertEquals(true, packet.failed)
        Assertions.assertEquals("OPTION__SET_USER_OPTION", packet.friendlyName)
    }
}
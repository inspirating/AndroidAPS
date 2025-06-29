package app.aaps.pump.danapen.comm

import app.aaps.pump.danapen.DanaPENTestBase
import dagger.android.AndroidInjector
import dagger.android.HasAndroidInjector
import org.junit.jupiter.api.Assertions
import org.junit.jupiter.api.Test

class DanaPenPacketEtcSetHistorySaveTest : DanaPENTestBase() {

    private val packetInjector = HasAndroidInjector {
        AndroidInjector {
            if (it is DanaPENPacketEtcSetHistorySave) {
                it.aapsLogger = aapsLogger
            }
        }
    }

    @Test fun runTest() {
        val packet = DanaPENPacketEtcSetHistorySave(packetInjector, 0, 0, 0, 0, 0, 0, 0, 0, 2)
        // test params
        val testParams = packet.getRequestParams()
        Assertions.assertEquals(2.toByte(), testParams[8])
        Assertions.assertEquals((2 ushr 8).toByte(), testParams[9])
        // test message decoding
        packet.handleMessage(createArray(34, 0.toByte()))
        Assertions.assertEquals(false, packet.failed)
        packet.handleMessage(createArray(34, 1.toByte()))
        Assertions.assertEquals(true, packet.failed)
        Assertions.assertEquals("ETC__SET_HISTORY_SAVE", packet.friendlyName)
    }
}
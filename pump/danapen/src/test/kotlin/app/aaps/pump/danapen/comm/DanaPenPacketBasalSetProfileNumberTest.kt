package app.aaps.pump.danapen.comm

import app.aaps.pump.danapen.DanaPENTestBase
import app.aaps.pump.danapen.comm.basal.DanaPENPacketBasalProfileNumberSet
import dagger.android.AndroidInjector
import dagger.android.HasAndroidInjector
import org.junit.jupiter.api.Assertions
import org.junit.jupiter.api.Test

class DanaPenPacketBasalSetProfileNumberTest : DanaPENTestBase() {

    private val packetInjector = HasAndroidInjector {
        AndroidInjector {
            if (it is DanaPENPacketBasalProfileNumberSet) {
                it.aapsLogger = aapsLogger
            }
        }
    }

    @Test fun runTest() {
        val packet = DanaPENPacketBasalProfileNumberSet(packetInjector, 1)
        // test params
        val testParams = packet.getRequestParams()
        // is profile 1
        Assertions.assertEquals(1.toByte(), testParams[0])
        // test message decoding
        packet.handleMessage(byteArrayOf(0.toByte(), 0.toByte(), 0.toByte()))
        Assertions.assertEquals(false, packet.failed)
        packet.handleMessage(byteArrayOf(0.toByte(), 0.toByte(), 1.toByte()))
        Assertions.assertEquals(true, packet.failed)
        Assertions.assertEquals("BASAL__SET_PROFILE_NUMBER", packet.friendlyName)
    }
}
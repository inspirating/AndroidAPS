package app.aaps.pump.danapen.comm

import app.aaps.pump.danapen.DanaPENTestBase
import app.aaps.pump.danapen.comm.basal.DanaPENPacketBasalGetProfileNumber
import dagger.android.AndroidInjector
import dagger.android.HasAndroidInjector
import org.junit.jupiter.api.Assertions
import org.junit.jupiter.api.Test

class DanaPenPacketBasalGetProfileNumberTest : DanaPENTestBase() {

    private val packetInjector = HasAndroidInjector {
        AndroidInjector {
            if (it is DanaPENPacketBasalGetProfileNumber) {
                it.aapsLogger = aapsLogger
                it.danaPump = danaPump
            }
        }
    }

    @Test fun runTest() {
        val packet = DanaPENPacketBasalGetProfileNumber(packetInjector)

        val array = ByteArray(100)
        putByteToArray(array, 0, 1.toByte())
        packet.handleMessage(array)
        Assertions.assertEquals(1, danaPump.activeProfile)
        Assertions.assertEquals("BASAL__GET_PROFILE_NUMBER", packet.friendlyName)
    }
}
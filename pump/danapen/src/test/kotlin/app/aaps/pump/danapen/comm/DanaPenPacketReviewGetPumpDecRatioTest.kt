package app.aaps.pump.danapen.comm

import app.aaps.pump.danapen.DanaPENTestBase
import app.aaps.pump.danapen.comm.pumpinfo.v3.DanaPENPacketReviewGetPumpDecRatio
import dagger.android.AndroidInjector
import dagger.android.HasAndroidInjector
import org.junit.jupiter.api.Assertions
import org.junit.jupiter.api.Test

class DanaPenPacketReviewGetPumpDecRatioTest : DanaPENTestBase() {

    private val packetInjector = HasAndroidInjector {
        AndroidInjector {
            if (it is DanaPENPacketReviewGetPumpDecRatio) {
                it.aapsLogger = aapsLogger
                it.danaPump = danaPump
            }
        }
    }

    @Test fun runTest() {
        val packet = DanaPENPacketReviewGetPumpDecRatio(packetInjector)

        val array = ByteArray(100)
        putByteToArray(array, 0, 4.toByte())
        packet.handleMessage(array)
        Assertions.assertEquals(20, danaPump.decRatio)
        Assertions.assertEquals("REVIEW__GET_PUMP_DEC_RATIO", packet.friendlyName)
    }
}
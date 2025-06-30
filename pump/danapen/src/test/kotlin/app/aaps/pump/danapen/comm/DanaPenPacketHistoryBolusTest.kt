package app.aaps.pump.danapen.comm

import app.aaps.pump.danapen.DanaPENTestBase
import app.aaps.pump.danapen.comm.history.DanaPENPacketHistoryBolus
import dagger.android.AndroidInjector
import dagger.android.HasAndroidInjector
import org.junit.jupiter.api.Assertions
import org.junit.jupiter.api.Test

class DanaPenPacketHistoryBolusTest : DanaPENTestBase() {

    private val packetInjector = HasAndroidInjector {
        AndroidInjector {
            if (it is DanaPENPacket) {
                it.aapsLogger = aapsLogger
                it.dateUtil = dateUtil
            }
            if (it is DanaPENPacketHistoryBolus) {
                it.rxBus = rxBus
            }
        }
    }

    @Test fun runTest() {
        val packet = DanaPENPacketHistoryBolus(packetInjector, System.currentTimeMillis())
        Assertions.assertEquals("REVIEW__BOLUS", packet.friendlyName)
    }
}
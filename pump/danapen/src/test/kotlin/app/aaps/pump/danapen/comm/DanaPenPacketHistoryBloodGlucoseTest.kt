package app.aaps.pump.danapen.comm

import app.aaps.pump.danapen.DanaPENTestBase
import app.aaps.pump.danapen.comm.history.DanaPENPacketHistoryBloodGlucose
import dagger.android.AndroidInjector
import dagger.android.HasAndroidInjector
import org.junit.jupiter.api.Assertions
import org.junit.jupiter.api.Test

class DanaPenPacketHistoryBloodGlucoseTest : DanaPENTestBase() {

    private val packetInjector = HasAndroidInjector {
        AndroidInjector {
            if (it is DanaPENPacket) {
                it.aapsLogger = aapsLogger
                it.dateUtil = dateUtil
            }
            if (it is DanaPENPacketHistoryBloodGlucose) {
                it.rxBus = rxBus
            }
        }
    }

    @Test fun runTest() {
        val packet = DanaPENPacketHistoryBloodGlucose(packetInjector, System.currentTimeMillis())
        Assertions.assertEquals("REVIEW__BLOOD_GLUCOSE", packet.friendlyName)
    }
}
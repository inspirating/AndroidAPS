package app.aaps.pump.danapen.comm

import app.aaps.pump.danapen.DanaPENTestBase
import dagger.android.AndroidInjector
import dagger.android.HasAndroidInjector
import org.junit.jupiter.api.Assertions
import org.junit.jupiter.api.Test

class DanaPenPacketGeneralSetHistoryUploadModeTest : DanaPENTestBase() {

    private val packetInjector = HasAndroidInjector {
        AndroidInjector {
            if (it is DanaPENPacketGeneralSetHistoryUploadMode) {
                it.aapsLogger = aapsLogger
            }
        }
    }

    @Test fun runTest() {
        val packet = DanaPENPacketGeneralSetHistoryUploadMode(packetInjector, 1)
        // test params
        Assertions.assertEquals(1.toByte(), packet.getRequestParams()[0])
        // test message decoding
        packet.handleMessage(createArray(3, 0.toByte()))
        Assertions.assertEquals(false, packet.failed)
        // everything ok :)
        packet.handleMessage(createArray(17, 1.toByte()))
        Assertions.assertEquals(true, packet.failed)
        Assertions.assertEquals("REVIEW__SET_HISTORY_UPLOAD_MODE", packet.friendlyName)
    }
}
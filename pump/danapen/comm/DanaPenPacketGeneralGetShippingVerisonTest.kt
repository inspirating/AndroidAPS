package app.aaps.pump.danapen.comm

import app.aaps.pump.danapen.DanaPENTestBase
import dagger.android.AndroidInjector
import dagger.android.HasAndroidInjector
import org.junit.jupiter.api.Assertions
import org.junit.jupiter.api.Test

class DanaPenPacketGeneralGetShippingVersionTest : DanaPENTestBase() {

    private val packetInjector = HasAndroidInjector {
        AndroidInjector {
            if (it is DanaPENPacketGeneralGetShippingVersion) {
                it.aapsLogger = aapsLogger
                it.danaPump = danaPump
            }
        }
    }

    @Test fun runTest() {
        val packet = DanaPENPacketGeneralGetShippingVersion(packetInjector)
        // test message decoding
        val ver = byteArrayOf((-78).toByte(), (-127).toByte(), (66).toByte(), (80).toByte(), (78).toByte(), (45).toByte(), (51).toByte(), (46).toByte(), (48).toByte(), (46).toByte(), (48).toByte())
        packet.handleMessage(ver)
        Assertions.assertFalse(packet.failed)
        Assertions.assertEquals("BPN-3.0.0", danaPump.bleModel)
        Assertions.assertEquals("GENERAL__GET_SHIPPING_VERSION", packet.friendlyName)
    }
}
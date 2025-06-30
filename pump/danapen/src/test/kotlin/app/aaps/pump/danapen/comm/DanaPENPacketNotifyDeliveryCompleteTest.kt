package app.aaps.pump.danapen.comm

import app.aaps.core.interfaces.rx.events.EventOverviewBolusProgress
import app.aaps.pump.danapen.DanaPENTestBase
import app.aaps.pump.danapen.comm.notify.DanaPENPacketNotifyDeliveryComplete
import dagger.android.AndroidInjector
import dagger.android.HasAndroidInjector
import org.junit.jupiter.api.Assertions
import org.junit.jupiter.api.Test
import org.mockito.Mockito.anyDouble
import org.mockito.Mockito.anyInt
import org.mockito.Mockito.`when`

class DanaPENPacketNotifyDeliveryCompleteTest : DanaPENTestBase() {

    private val packetInjector = HasAndroidInjector {
        AndroidInjector {
            if (it is DanaPENPacketNotifyDeliveryComplete) {
                it.aapsLogger = aapsLogger
                it.rxBus = rxBus
                it.rh = rh
                it.danaPump = danaPump
            }
        }
    }

    @Test fun runTest() {
        `when`(rh.gs(anyInt(), anyDouble())).thenReturn("SomeString")

        danaPump.bolusingTreatment = EventOverviewBolusProgress.Treatment(0.0, 0, true, 0)
        val packet = DanaPENPacketNotifyDeliveryComplete(packetInjector)
        // test params
        Assertions.assertEquals(0, packet.getRequestParams().size)
        // test message decoding
        packet.handleMessage(createArray(17, 0.toByte()))
        Assertions.assertEquals(true, danaPump.bolusDone)
        Assertions.assertEquals("NOTIFY__DELIVERY_COMPLETE", packet.friendlyName)
    }
}
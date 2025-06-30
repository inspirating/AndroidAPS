package app.aaps.pump.danapen.comm

import app.aaps.core.interfaces.pump.DetailedBolusInfoStorage
import app.aaps.core.interfaces.pump.PumpSync
import app.aaps.core.interfaces.pump.TemporaryBasalStorage
import app.aaps.pump.danapen.DanaPENTestBase
import app.aaps.pump.danapen.comm.history.DanaPENPacketAPSHistoryEvents
import dagger.android.AndroidInjector
import dagger.android.HasAndroidInjector
import org.junit.jupiter.api.Assertions
import org.junit.jupiter.api.Test
import org.mockito.Mock
import java.util.Calendar
import java.util.GregorianCalendar

class DanaPenPacketApsHistoryEventsTest : DanaPENTestBase() {

    @Mock lateinit var pumpSync: PumpSync
    @Mock lateinit var detailedBolusInfoStorage: DetailedBolusInfoStorage
    @Mock lateinit var temporaryBasalStorage: TemporaryBasalStorage

    private val packetInjector = HasAndroidInjector {
        AndroidInjector {
            if (it is DanaPENPacket) {
                it.aapsLogger = aapsLogger
                it.dateUtil = dateUtil
            }
            if (it is DanaPENPacketAPSHistoryEvents) {
                it.rxBus = rxBus
                it.rh = rh
                it.pumpSync = pumpSync
                it.danaPump = danaPump
                it.detailedBolusInfoStorage = detailedBolusInfoStorage
                it.temporaryBasalStorage = temporaryBasalStorage
                it.preferences = preferences
            }
        }
    }

    @Test fun runTest() {
        val now = dateUtil.now()

        val testPacket = DanaPENPacketAPSHistoryEvents(packetInjector, now)
        // test getRequestedParams
        val returnedValues = testPacket.getRequestParams()
        val expectedValues = getCalender(now)
        //year
        Assertions.assertEquals(expectedValues[0], returnedValues[0])
        //month
        Assertions.assertEquals(expectedValues[1], returnedValues[1])
        //day of month
        Assertions.assertEquals(expectedValues[2], returnedValues[2])
        // hour
        Assertions.assertEquals(expectedValues[3], returnedValues[3])
        // minute
        Assertions.assertEquals(expectedValues[4], returnedValues[4])
        // second
        Assertions.assertEquals(expectedValues[5], returnedValues[5])
        Assertions.assertEquals("APS_HISTORY_EVENTS", testPacket.friendlyName)
    }

    private fun getCalender(from: Long): ByteArray {
        val cal = GregorianCalendar()
        if (from != 0L) cal.timeInMillis = from else cal[2000, 0, 1, 0, 0] = 0
        val ret = ByteArray(6)
        ret[0] = (cal[Calendar.YEAR] - 1900 - 100 and 0xff).toByte()
        ret[1] = (cal[Calendar.MONTH] + 1 and 0xff).toByte()
        ret[2] = (cal[Calendar.DAY_OF_MONTH] and 0xff).toByte()
        ret[3] = (cal[Calendar.HOUR_OF_DAY] and 0xff).toByte()
        ret[4] = (cal[Calendar.MINUTE] and 0xff).toByte()
        ret[5] = (cal[Calendar.SECOND] and 0xff).toByte()
        return ret
    }
}
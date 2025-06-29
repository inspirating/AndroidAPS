package app.aaps.pump.danapen.comm

import app.aaps.core.interfaces.constraints.ConstraintsChecker
import app.aaps.core.objects.constraints.ConstraintObject
import app.aaps.pump.dana.DanaPump
import app.aaps.pump.danapen.DanaPENTestBase
import dagger.android.AndroidInjector
import dagger.android.HasAndroidInjector
import info.nightscout.androidaps.danapen.encryption.BleEncryption
import org.junit.jupiter.api.Assertions
import org.junit.jupiter.api.Test
import org.mockito.Mock
import org.mockito.Mockito.`when`

class DanaPenMessageHashTableTest : DanaPENTestBase() {

    @Mock lateinit var constraintChecker: ConstraintsChecker

    private val packetInjector = HasAndroidInjector {
        AndroidInjector {
            if (it is DanaPENPacket) {
                it.aapsLogger = aapsLogger
                it.dateUtil = dateUtil
            }
            if (it is DanaPENPacketBolusSetStepBolusStart) {
                it.constraintChecker = constraintChecker
            }
            if (it is DanaPENPacketBolusSetStepBolusStart) {
                it.danaPump = danaPump
            }
            if (it is DanaPENPacketAPSHistoryEvents) {
                it.danaPump = danaPump
            }
        }
    }

    @Test
    fun runTest() {
        `when`(constraintChecker.applyBolusConstraints(anyObject())).thenReturn(ConstraintObject(0.0, aapsLogger))

        val danaPENMessageHashTable = DanaPENMessageHashTable(packetInjector)
        val forTesting: DanaPENPacket = DanaPENPacketAPSSetEventHistory(packetInjector, DanaPump.HistoryEntry.CARBS.value, 0, 0, 0)
        val testPacket: DanaPENPacket = danaPENMessageHashTable.findMessage(forTesting.command)
        Assertions.assertEquals(BleEncryption.DANAR_PACKET__OPCODE__APS_SET_EVENT_HISTORY.toLong(), testPacket.opCode.toLong())
    }
}
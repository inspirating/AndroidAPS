package app.aaps.pump.danapen.comm

import app.aaps.pump.danapen.comm.basal.DanaPENPacketAPSBasalSetTemporaryBasal
import app.aaps.pump.danapen.comm.basal.DanaPENPacketBasalGetBasalRate
import app.aaps.pump.danapen.comm.basal.DanaPENPacketBasalGetProfileNumber
import app.aaps.pump.danapen.comm.basal.DanaPENPacketBasalSetCancelTemporaryBasal
import app.aaps.pump.danapen.comm.basal.DanaPENPacketBasalSetProfileBasalRate
import app.aaps.pump.danapen.comm.basal.DanaPENPacketBasalSetProfileNumber
import app.aaps.pump.danapen.comm.basal.DanaPENPacketBasalSetSuspendOff
import app.aaps.pump.danapen.comm.basal.DanaPENPacketBasalSetSuspendOn
import app.aaps.pump.danapen.comm.basal.DanaPENPacketBasalSetTemporaryBasal
import app.aaps.pump.danapen.comm.ble.keep.DanaPENPacketEtcKeepConnection
import app.aaps.pump.danapen.comm.ble.keep.DanaPENPacketEtcSetHistorySave
import app.aaps.pump.danapen.comm.bolus.DanaPENPacketBolusGet24CIRCFArray
import app.aaps.pump.danapen.comm.bolus.DanaPENPacketBolusGetBolusOption
import app.aaps.pump.danapen.comm.bolus.DanaPENPacketBolusGetCIRCFArray
import app.aaps.pump.danapen.comm.bolus.DanaPENPacketBolusGetCalculationInformation
import app.aaps.pump.danapen.comm.bolus.DanaPENPacketBolusGetStepBolusInformation
import app.aaps.pump.danapen.comm.bolus.DanaPENPacketBolusSet24CIRCFArray
import app.aaps.pump.danapen.comm.bolus.DanaPENPacketBolusSetBolusOption
import app.aaps.pump.danapen.comm.bolus.DanaPENPacketBolusSetExtendedBolus
import app.aaps.pump.danapen.comm.bolus.DanaPENPacketBolusSetExtendedBolusCancel
import app.aaps.pump.danapen.comm.bolus.DanaPENPacketBolusSetStepBolusStart
import app.aaps.pump.danapen.comm.bolus.DanaPENPacketBolusSetStepBolusStop
import app.aaps.pump.danapen.comm.history.DanaPENPacketAPSHistoryEvents
import app.aaps.pump.danapen.comm.history.DanaPENPacketAPSSetEventHistory
import app.aaps.pump.danapen.comm.history.DanaPENPacketHistoryAlarm
import app.aaps.pump.danapen.comm.history.DanaPENPacketHistoryAllHistory
import app.aaps.pump.danapen.comm.history.DanaPENPacketHistoryBasal
import app.aaps.pump.danapen.comm.history.DanaPENPacketHistoryBloodGlucose
import app.aaps.pump.danapen.comm.history.DanaPENPacketHistoryBolus
import app.aaps.pump.danapen.comm.history.DanaPENPacketHistoryCarbohydrate
import app.aaps.pump.danapen.comm.history.DanaPENPacketHistoryDaily
import app.aaps.pump.danapen.comm.history.DanaPENPacketHistoryPrime
import app.aaps.pump.danapen.comm.history.DanaPENPacketHistoryRefill
import app.aaps.pump.danapen.comm.history.DanaPENPacketHistorySuspend
import app.aaps.pump.danapen.comm.history.DanaPENPacketHistoryTemporary
import app.aaps.pump.danapen.comm.notify.error.DanaPENPacketNotifyAlarm
import app.aaps.pump.danapen.comm.notify.DanaPENPacketNotifyDeliveryComplete
import app.aaps.pump.danapen.comm.notify.DanaPENPacketNotifyDeliveryRateDisplay
import app.aaps.pump.danapen.comm.notify.error.DanaPENPacketNotifyMissedBolusAlarm
import app.aaps.pump.danapen.comm.pumpinfo.DanaPENPacketGeneralGetPumpCheck
import app.aaps.pump.danapen.comm.pumpinfo.DanaPENPacketGeneralGetShippingInformation
import app.aaps.pump.danapen.comm.pumpinfo.DanaPENPacketGeneralGetShippingVersion
import app.aaps.pump.danapen.comm.pumpinfo.user.DanaPENPacketGeneralGetUserTimeChangeFlag
import app.aaps.pump.danapen.comm.pumpinfo.DanaPENPacketGeneralInitialScreenInformation
import app.aaps.pump.danapen.comm.pumpinfo.DanaPENPacketGeneralSetHistoryUploadMode
import app.aaps.pump.danapen.comm.pumpinfo.user.DanaPENPacketGeneralSetUserTimeChangeFlagClear
import app.aaps.pump.danapen.comm.pumpinfo.time.DanaPENPacketOptionGetPumpTime
import app.aaps.pump.danapen.comm.pumpinfo.time.DanaPENPacketOptionGetPumpUTCAndTimeZone
import app.aaps.pump.danapen.comm.pumpinfo.user.DanaPENPacketOptionGetUserOption
import app.aaps.pump.danapen.comm.pumpinfo.time.DanaPENPacketOptionSetPumpTime
import app.aaps.pump.danapen.comm.pumpinfo.time.DanaPENPacketOptionSetPumpUTCAndTimeZone
import app.aaps.pump.danapen.comm.pumpinfo.user.DanaPENPacketOptionSetUserOption
import app.aaps.pump.danapen.comm.pumpinfo.DanaPENPacketReviewBolusAvg
import app.aaps.pump.danapen.comm.pumpinfo.DanaPENPacketReviewGetPumpDecRatio
import dagger.android.HasAndroidInjector
import javax.inject.Inject
import javax.inject.Singleton

@Singleton
class DanaPENMessageHashTable @Inject constructor(
    val injector: HasAndroidInjector
) {

    var messages: HashMap<Int, DanaPENPacket> = HashMap()

    fun put(message: DanaPENPacket) {
        messages[message.command] = message
    }

    fun findMessage(command: Int): DanaPENPacket {
        return messages[command] ?: DanaPENPacket(injector)
    }

    init {
        put(DanaPENPacketBasalSetCancelTemporaryBasal(injector))
        put(DanaPENPacketBasalGetBasalRate(injector))
        put(DanaPENPacketBasalGetProfileNumber(injector))
        put(DanaPENPacketBasalSetProfileBasalRate(injector, 0, arrayOf()))
        put(DanaPENPacketBasalSetProfileNumber(injector))
        put(DanaPENPacketBasalSetSuspendOff(injector))
        put(DanaPENPacketBasalSetSuspendOn(injector))
        put(DanaPENPacketBasalSetTemporaryBasal(injector))
        put(DanaPENPacketBolusGetBolusOption(injector))
        put(DanaPENPacketBolusGetCalculationInformation(injector))
        put(DanaPENPacketBolusGetCIRCFArray(injector))
        put(DanaPENPacketBolusGetStepBolusInformation(injector))
        put(DanaPENPacketBolusSetBolusOption(injector))
        put(DanaPENPacketBolusSet24CIRCFArray(injector, null))
        put(DanaPENPacketBolusGet24CIRCFArray(injector))
        put(DanaPENPacketBolusSetExtendedBolus(injector))
        put(DanaPENPacketBolusSetExtendedBolusCancel(injector))
        put(DanaPENPacketBolusSetStepBolusStart(injector))
        put(DanaPENPacketBolusSetStepBolusStop(injector))
        put(DanaPENPacketEtcKeepConnection(injector))
        put(DanaPENPacketEtcSetHistorySave(injector))
        put(DanaPENPacketGeneralInitialScreenInformation(injector))
        put(DanaPENPacketNotifyAlarm(injector))
        put(DanaPENPacketNotifyDeliveryComplete(injector))
        put(DanaPENPacketNotifyDeliveryRateDisplay(injector))
        put(DanaPENPacketNotifyMissedBolusAlarm(injector))
        put(DanaPENPacketOptionGetPumpTime(injector))
        put(DanaPENPacketOptionGetPumpUTCAndTimeZone(injector))
        put(DanaPENPacketOptionGetUserOption(injector))
        put(DanaPENPacketOptionSetPumpTime(injector))
        put(DanaPENPacketOptionSetPumpUTCAndTimeZone(injector))
        put(DanaPENPacketOptionSetUserOption(injector))
        //put(new DanaPEN_Packet_History_(injector));
        put(DanaPENPacketHistoryAlarm(injector))
        put(DanaPENPacketHistoryAllHistory(injector))
        put(DanaPENPacketHistoryBasal(injector))
        put(DanaPENPacketHistoryBloodGlucose(injector))
        put(DanaPENPacketHistoryBolus(injector))
        put(DanaPENPacketReviewBolusAvg(injector))
        put(DanaPENPacketHistoryCarbohydrate(injector))
        put(DanaPENPacketHistoryDaily(injector))
        put(DanaPENPacketHistoryPrime(injector))
        put(DanaPENPacketHistoryRefill(injector))
        put(DanaPENPacketHistorySuspend(injector))
        put(DanaPENPacketHistoryTemporary(injector))
        put(DanaPENPacketGeneralGetPumpCheck(injector))
        put(DanaPENPacketGeneralGetShippingInformation(injector))
        put(DanaPENPacketGeneralGetUserTimeChangeFlag(injector))
        put(DanaPENPacketGeneralSetHistoryUploadMode(injector))
        put(DanaPENPacketGeneralSetUserTimeChangeFlagClear(injector))
        // APS
        put(DanaPENPacketAPSBasalSetTemporaryBasal(injector, 0))
        put(DanaPENPacketAPSHistoryEvents(injector, 0))
        put(DanaPENPacketAPSSetEventHistory(injector, 0, 0, 0, 0))
        // v3
        put(DanaPENPacketGeneralGetShippingVersion(injector))
        put(DanaPENPacketReviewGetPumpDecRatio(injector))
    }
}
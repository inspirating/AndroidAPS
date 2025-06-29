package app.aaps.pump.danapen.comm

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
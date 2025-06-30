package app.aaps.pump.danapen.di

import app.aaps.pump.danapen.comm.DanaPENPacket
import app.aaps.pump.danapen.comm.basal.DanaPENPacketAPSBasalSetTemporaryBasal
import app.aaps.pump.danapen.comm.history.DanaPENPacketAPSHistoryEvents
import app.aaps.pump.danapen.comm.history.DanaPENPacketAPSSetEventHistory
import app.aaps.pump.danapen.comm.basal.DanaPENPacketBasalGetBasalRate
import app.aaps.pump.danapen.comm.basal.DanaPENPacketBasalGetProfileNumber
import app.aaps.pump.danapen.comm.basal.DanaPENPacketBasalSetCancelTemporaryBasal
import app.aaps.pump.danapen.comm.basal.DanaPENPacketBasalSetProfileBasalRate
import app.aaps.pump.danapen.comm.basal.DanaPENPacketBasalSetProfileNumber
import app.aaps.pump.danapen.comm.basal.DanaPENPacketBasalSetSuspendOff
import app.aaps.pump.danapen.comm.basal.DanaPENPacketBasalSetSuspendOn
import app.aaps.pump.danapen.comm.basal.DanaPENPacketBasalSetTemporaryBasal
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
import app.aaps.pump.danapen.comm.ble.keep.DanaPENPacketEtcKeepConnection
import app.aaps.pump.danapen.comm.ble.keep.DanaPENPacketEtcSetHistorySave
import app.aaps.pump.danapen.comm.pumpinfo.DanaPENPacketGeneralGetPumpCheck
import app.aaps.pump.danapen.comm.pumpinfo.DanaPENPacketGeneralGetShippingInformation
import app.aaps.pump.danapen.comm.pumpinfo.v3.DanaPENPacketGeneralGetShippingVersion
import app.aaps.pump.danapen.comm.pumpinfo.user.DanaPENPacketGeneralGetUserTimeChangeFlag
import app.aaps.pump.danapen.comm.pumpinfo.DanaPENPacketGeneralInitialScreenInformation
import app.aaps.pump.danapen.comm.pumpinfo.DanaPENPacketGeneralSetHistoryUploadMode
import app.aaps.pump.danapen.comm.pumpinfo.user.DanaPENPacketGeneralSetUserTimeChangeFlagClear
import app.aaps.pump.danapen.comm.history.DanaPENPacketHistory
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
import app.aaps.pump.danapen.comm.pumpinfo.time.DanaPENPacketOptionGetPumpTime
import app.aaps.pump.danapen.comm.pumpinfo.time.DanaPENPacketOptionGetPumpUTCAndTimeZone
import app.aaps.pump.danapen.comm.pumpinfo.user.DanaPENPacketOptionGetUserOption
import app.aaps.pump.danapen.comm.pumpinfo.time.DanaPENPacketOptionSetPumpTime
import app.aaps.pump.danapen.comm.pumpinfo.time.DanaPENPacketOptionSetPumpUTCAndTimeZone
import app.aaps.pump.danapen.comm.pumpinfo.user.DanaPENPacketOptionSetUserOption
import app.aaps.pump.danapen.comm.pumpinfo.DanaPENPacketReviewBolusAvg
import app.aaps.pump.danapen.comm.pumpinfo.v3.DanaPENPacketReviewGetPumpDecRatio
import dagger.Module
import dagger.android.ContributesAndroidInjector

@Module
@Suppress("unused")
abstract class DanaPENCommModule {

    @ContributesAndroidInjector abstract fun contributesDanaPENPacket(): DanaPENPacket
    @ContributesAndroidInjector abstract fun contributesDanaPENPacketBasalSetCancelTemporaryBasal(): DanaPENPacketBasalSetCancelTemporaryBasal
    @ContributesAndroidInjector abstract fun contributesDanaPENPacketBasalGetBasalRate(): DanaPENPacketBasalGetBasalRate
    @ContributesAndroidInjector abstract fun contributesDanaPENPacketBasalGetProfileNumber(): DanaPENPacketBasalGetProfileNumber
    @ContributesAndroidInjector abstract fun contributesDanaPENPacketBasalSetProfileBasalRate(): DanaPENPacketBasalSetProfileBasalRate
    @ContributesAndroidInjector abstract fun contributesDanaPENPacketBasalSetProfileNumber(): DanaPENPacketBasalSetProfileNumber
    @ContributesAndroidInjector abstract fun contributesDanaPENPacketBasalSetSuspendOff(): DanaPENPacketBasalSetSuspendOff
    @ContributesAndroidInjector abstract fun contributesDanaPENPacketBasalSetSuspendOn(): DanaPENPacketBasalSetSuspendOn
    @ContributesAndroidInjector abstract fun contributesDanaPENPacketBasalSetTemporaryBasal(): DanaPENPacketBasalSetTemporaryBasal
    @ContributesAndroidInjector abstract fun contributesDanaPENPacketBolusGetBolusOption(): DanaPENPacketBolusGetBolusOption
    @ContributesAndroidInjector abstract fun contributesDanaPENPacketBolusGetCalculationInformation(): DanaPENPacketBolusGetCalculationInformation
    @ContributesAndroidInjector abstract fun contributesDanaPENPacketBolusGetCIRCFArray(): DanaPENPacketBolusGetCIRCFArray
    @ContributesAndroidInjector abstract fun contributesDanaPENPacketBolusGetStepBolusInformation(): DanaPENPacketBolusGetStepBolusInformation
    @ContributesAndroidInjector abstract fun contributesDanaPENPacketBolusSetBolusOption(): DanaPENPacketBolusSetBolusOption
    @ContributesAndroidInjector abstract fun contributesDanaPENPacketBolusGet24CIRCFArray(): DanaPENPacketBolusGet24CIRCFArray
    @ContributesAndroidInjector abstract fun contributesDanaPENPacketBolusSet24CIRCFArray(): DanaPENPacketBolusSet24CIRCFArray
    @ContributesAndroidInjector abstract fun contributesDanaPENPacketBolusSetExtendedBolus(): DanaPENPacketBolusSetExtendedBolus
    @ContributesAndroidInjector abstract fun contributesDanaPENPacketBolusSetExtendedBolusCancel(): DanaPENPacketBolusSetExtendedBolusCancel
    @ContributesAndroidInjector abstract fun contributesDanaPENPacketBolusSetStepBolusStart(): DanaPENPacketBolusSetStepBolusStart
    @ContributesAndroidInjector abstract fun contributesDanaPENPacketBolusSetStepBolusStop(): DanaPENPacketBolusSetStepBolusStop
    @ContributesAndroidInjector abstract fun contributesDanaPENPacketEtcKeepConnection(): DanaPENPacketEtcKeepConnection
    @ContributesAndroidInjector abstract fun contributesDanaPENPacketEtcSetHistorySave(): DanaPENPacketEtcSetHistorySave
    @ContributesAndroidInjector abstract fun contributesDanaPENPacketGeneralInitialScreenInformation(): DanaPENPacketGeneralInitialScreenInformation
    @ContributesAndroidInjector abstract fun contributesDanaPENPacketNotifyDeliveryRateDisplay(): DanaPENPacketNotifyDeliveryRateDisplay
    @ContributesAndroidInjector abstract fun contributesDanaPENPacketNotifyAlarm(): DanaPENPacketNotifyAlarm
    @ContributesAndroidInjector abstract fun contributesDanaPENPacketNotifyDeliveryComplete(): DanaPENPacketNotifyDeliveryComplete
    @ContributesAndroidInjector abstract fun contributesDanaPENPacketNotifyMissedBolusAlarm(): DanaPENPacketNotifyMissedBolusAlarm
    @ContributesAndroidInjector abstract fun contributesDanaPENPacketOptionGetPumpTime(): DanaPENPacketOptionGetPumpTime
    @ContributesAndroidInjector abstract fun contributesDanaPENPacketOptionGetUserOption(): DanaPENPacketOptionGetUserOption
    @ContributesAndroidInjector abstract fun contributesDanaPENPacketOptionSetPumpTime(): DanaPENPacketOptionSetPumpTime
    @ContributesAndroidInjector abstract fun contributesDanaPENPacketOptionSetUserOption(): DanaPENPacketOptionSetUserOption
    @ContributesAndroidInjector abstract fun contributesDanaPENPacketHistory(): DanaPENPacketHistory
    @ContributesAndroidInjector abstract fun contributesDanaPENPacketHistoryAlarm(): DanaPENPacketHistoryAlarm
    @ContributesAndroidInjector abstract fun contributesDanaPENPacketHistoryAllHistory(): DanaPENPacketHistoryAllHistory
    @ContributesAndroidInjector abstract fun contributesDanaPENPacketHistoryBasal(): DanaPENPacketHistoryBasal
    @ContributesAndroidInjector abstract fun contributesDanaPENPacketHistoryBloodGlucose(): DanaPENPacketHistoryBloodGlucose
    @ContributesAndroidInjector abstract fun contributesDanaPENPacketHistoryBolus(): DanaPENPacketHistoryBolus
    @ContributesAndroidInjector abstract fun contributesDanaPENPacketReviewBolusAvg(): DanaPENPacketReviewBolusAvg
    @ContributesAndroidInjector abstract fun contributesDanaPENPacketHistoryCarbohydrate(): DanaPENPacketHistoryCarbohydrate
    @ContributesAndroidInjector abstract fun contributesDanaPENPacketHistoryDaily(): DanaPENPacketHistoryDaily
    @ContributesAndroidInjector abstract fun contributesDanaPENPacketGeneralGetPumpCheck(): DanaPENPacketGeneralGetPumpCheck
    @ContributesAndroidInjector abstract fun contributesDanaPENPacketGeneralGetShippingInformation(): DanaPENPacketGeneralGetShippingInformation
    @ContributesAndroidInjector abstract fun contributesDanaPENPacketGeneralGetUserTimeChangeFlag(): DanaPENPacketGeneralGetUserTimeChangeFlag
    @ContributesAndroidInjector abstract fun contributesDanaPENPacketHistoryPrime(): DanaPENPacketHistoryPrime
    @ContributesAndroidInjector abstract fun contributesDanaPENPacketHistoryRefill(): DanaPENPacketHistoryRefill
    @ContributesAndroidInjector abstract fun contributesDanaPENPacketGeneralSetHistoryUploadMode(): DanaPENPacketGeneralSetHistoryUploadMode
    @ContributesAndroidInjector abstract fun contributesDanaPENPacketGeneralSetUserTimeChangeFlagClear(): DanaPENPacketGeneralSetUserTimeChangeFlagClear
    @ContributesAndroidInjector abstract fun contributesDanaPENPacketHistorySuspend(): DanaPENPacketHistorySuspend
    @ContributesAndroidInjector abstract fun contributesDanaPENPacketHistoryTemporary(): DanaPENPacketHistoryTemporary
    @ContributesAndroidInjector abstract fun contributesDanaPENPacketAPSBasalSetTemporaryBasal(): DanaPENPacketAPSBasalSetTemporaryBasal
    @ContributesAndroidInjector abstract fun contributesDanaPENPacketAPSHistoryEvents(): DanaPENPacketAPSHistoryEvents
    @ContributesAndroidInjector abstract fun contributesDanaPENPacketAPSSetEventHistory(): DanaPENPacketAPSSetEventHistory
    @ContributesAndroidInjector abstract fun contributesDanaPENPacketGeneralGetShippingVersion(): DanaPENPacketGeneralGetShippingVersion
    @ContributesAndroidInjector abstract fun contributesDanaPENPacketReviewGetPumpDecRatio(): DanaPENPacketReviewGetPumpDecRatio
    @ContributesAndroidInjector abstract fun contributesDanaPENPacketOptionGetPumpUTCAndTimeZone(): DanaPENPacketOptionGetPumpUTCAndTimeZone
    @ContributesAndroidInjector abstract fun contributesDanaPENPacketOptionSetPumpUTCAndTimeZone(): DanaPENPacketOptionSetPumpUTCAndTimeZone
}
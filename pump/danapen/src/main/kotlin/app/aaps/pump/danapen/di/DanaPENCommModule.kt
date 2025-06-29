package app.aaps.pump.danapen.di

import app.aaps.pump.danapen.comm.DanaPENPacket
import app.aaps.pump.danapen.comm.DanaPENPacketAPSBasalSetTemporaryBasal
import app.aaps.pump.danapen.comm.DanaPENPacketAPSHistoryEvents
import app.aaps.pump.danapen.comm.DanaPENPacketAPSSetEventHistory
import app.aaps.pump.danapen.comm.DanaPENPacketBasalGetBasalRate
import app.aaps.pump.danapen.comm.DanaPENPacketBasalGetProfileNumber
import app.aaps.pump.danapen.comm.DanaPENPacketBasalSetCancelTemporaryBasal
import app.aaps.pump.danapen.comm.DanaPENPacketBasalSetProfileBasalRate
import app.aaps.pump.danapen.comm.DanaPENPacketBasalSetProfileNumber
import app.aaps.pump.danapen.comm.DanaPENPacketBasalSetSuspendOff
import app.aaps.pump.danapen.comm.DanaPENPacketBasalSetSuspendOn
import app.aaps.pump.danapen.comm.DanaPENPacketBasalSetTemporaryBasal
import app.aaps.pump.danapen.comm.DanaPENPacketBolusGet24CIRCFArray
import app.aaps.pump.danapen.comm.DanaPENPacketBolusGetBolusOption
import app.aaps.pump.danapen.comm.DanaPENPacketBolusGetCIRCFArray
import app.aaps.pump.danapen.comm.DanaPENPacketBolusGetCalculationInformation
import app.aaps.pump.danapen.comm.DanaPENPacketBolusGetStepBolusInformation
import app.aaps.pump.danapen.comm.DanaPENPacketBolusSet24CIRCFArray
import app.aaps.pump.danapen.comm.DanaPENPacketBolusSetBolusOption
import app.aaps.pump.danapen.comm.DanaPENPacketBolusSetExtendedBolus
import app.aaps.pump.danapen.comm.DanaPENPacketBolusSetExtendedBolusCancel
import app.aaps.pump.danapen.comm.DanaPENPacketBolusSetStepBolusStart
import app.aaps.pump.danapen.comm.DanaPENPacketBolusSetStepBolusStop
import app.aaps.pump.danapen.comm.DanaPENPacketEtcKeepConnection
import app.aaps.pump.danapen.comm.DanaPENPacketEtcSetHistorySave
import app.aaps.pump.danapen.comm.DanaPENPacketGeneralGetPumpCheck
import app.aaps.pump.danapen.comm.DanaPENPacketGeneralGetShippingInformation
import app.aaps.pump.danapen.comm.DanaPENPacketGeneralGetShippingVersion
import app.aaps.pump.danapen.comm.DanaPENPacketGeneralGetUserTimeChangeFlag
import app.aaps.pump.danapen.comm.DanaPENPacketGeneralInitialScreenInformation
import app.aaps.pump.danapen.comm.DanaPENPacketGeneralSetHistoryUploadMode
import app.aaps.pump.danapen.comm.DanaPENPacketGeneralSetUserTimeChangeFlagClear
import app.aaps.pump.danapen.comm.DanaPENPacketHistory
import app.aaps.pump.danapen.comm.DanaPENPacketHistoryAlarm
import app.aaps.pump.danapen.comm.DanaPENPacketHistoryAllHistory
import app.aaps.pump.danapen.comm.DanaPENPacketHistoryBasal
import app.aaps.pump.danapen.comm.DanaPENPacketHistoryBloodGlucose
import app.aaps.pump.danapen.comm.DanaPENPacketHistoryBolus
import app.aaps.pump.danapen.comm.DanaPENPacketHistoryCarbohydrate
import app.aaps.pump.danapen.comm.DanaPENPacketHistoryDaily
import app.aaps.pump.danapen.comm.DanaPENPacketHistoryPrime
import app.aaps.pump.danapen.comm.DanaPENPacketHistoryRefill
import app.aaps.pump.danapen.comm.DanaPENPacketHistorySuspend
import app.aaps.pump.danapen.comm.DanaPENPacketHistoryTemporary
import app.aaps.pump.danapen.comm.DanaPENPacketNotifyAlarm
import app.aaps.pump.danapen.comm.DanaPENPacketNotifyDeliveryComplete
import app.aaps.pump.danapen.comm.DanaPENPacketNotifyDeliveryRateDisplay
import app.aaps.pump.danapen.comm.DanaPENPacketNotifyMissedBolusAlarm
import app.aaps.pump.danapen.comm.DanaPENPacketOptionGetPumpTime
import app.aaps.pump.danapen.comm.DanaPENPacketOptionGetPumpUTCAndTimeZone
import app.aaps.pump.danapen.comm.DanaPENPacketOptionGetUserOption
import app.aaps.pump.danapen.comm.DanaPENPacketOptionSetPumpTime
import app.aaps.pump.danapen.comm.DanaPENPacketOptionSetPumpUTCAndTimeZone
import app.aaps.pump.danapen.comm.DanaPENPacketOptionSetUserOption
import app.aaps.pump.danapen.comm.DanaPENPacketReviewBolusAvg
import app.aaps.pump.danapen.comm.DanaPENPacketReviewGetPumpDecRatio
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
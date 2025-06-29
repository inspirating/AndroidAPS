package app.aaps.pump.danapen.di

import app.aaps.pump.danapen.activities.BLEScanActivity
import app.aaps.pump.danapen.activities.EnterPinActivity
import app.aaps.pump.danapen.activities.PairingHelperActivity
import app.aaps.pump.danapen.dialogs.PairingProgressDialog
import dagger.Module
import dagger.android.ContributesAndroidInjector

@Module
@Suppress("unused")
abstract class DanaPENActivitiesModule {

    @ContributesAndroidInjector abstract fun contributesBLEScanActivity(): BLEScanActivity
    @ContributesAndroidInjector abstract fun contributesPairingHelperActivity(): PairingHelperActivity
    @ContributesAndroidInjector abstract fun contributeEnterPinActivity(): EnterPinActivity

    @ContributesAndroidInjector abstract fun contributesPairingProgressDialog(): PairingProgressDialog
}
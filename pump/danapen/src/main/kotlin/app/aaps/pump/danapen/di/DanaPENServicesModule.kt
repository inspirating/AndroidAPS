package app.aaps.pump.danapen.di

import app.aaps.pump.danapen.services.DanaPENService
import dagger.Module
import dagger.android.ContributesAndroidInjector

@Module
@Suppress("unused")
abstract class DanaPENServicesModule {

    @ContributesAndroidInjector abstract fun contributesDanaPENService(): DanaPENService
}
package app.aaps.pump.danapen.di

import dagger.Module

@Module(
    includes = [
        DanaPENCommModule::class,
        DanaPENActivitiesModule::class,
        DanaPENServicesModule::class
    ]
)
open class DanaPENModule
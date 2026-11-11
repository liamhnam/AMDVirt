import SwiftUI

@main
struct AMDVirtManagerApp: App {
    @StateObject private var model = Model()

    var body: some Scene {
        WindowGroup("AMDVirt") {
            ContentView()
                .environmentObject(model)
                .frame(minWidth: 760, minHeight: 520)
                .onAppear {
                    model.refresh()
                    model.checkLatest()
                }
        }
    }
}

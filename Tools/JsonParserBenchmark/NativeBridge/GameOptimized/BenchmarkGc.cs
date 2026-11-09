using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

// 計測用プロジェクトだけに含め、ゲーム用DLLの公開APIには追加しない
internal static class BenchmarkGc
{
    [UnmanagedCallersOnly(EntryPoint = "Collect", CallConvs = [typeof(CallConvCdecl)])]
    public static void Collect()
    {
        GC.Collect();
        GC.WaitForPendingFinalizers();
        GC.Collect();
    }
}

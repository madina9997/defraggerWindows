using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Runtime.InteropServices;

namespace PInvokeDefragLibrary
{
    //public class PInvokeClass
    //{
        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
        public struct TVolume
        {
            [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 261)]
            public string Name;
        }
        [StructLayout(LayoutKind.Sequential)]
        public struct TQuad
        {
            public UInt64 State;
            public ulong BeginCluster, EndCluster;

            public override string ToString() => $"{BeginCluster} -- {EndCluster} [{State}]";
        }

        public static class PInvoke
        {
            [DllImport("DefragLibrary.dll", EntryPoint = "GetAllVolumesArray",
            CallingConvention = CallingConvention.StdCall)]
            private static extern int GetAllVolumesArray(
            int len, [MarshalAs(UnmanagedType.LPArray, SizeParamIndex = 0)] [In, Out] TVolume[] array
            );
            public static TVolume[] GetAllVolumes()
            {
                TVolume[] volumes = new TVolume[10];
                int max = GetAllVolumesArray(10, volumes);
                return volumes;
            }

            [DllImport("DefragLibrary.dll", EntryPoint = "GetAllVolumesArrayGUID",
                CallingConvention = CallingConvention.StdCall)]
            private static extern int GetAllVolumesArrayGUID(
                int len, [MarshalAs(UnmanagedType.LPArray, SizeParamIndex = 0)] [In, Out] TVolume[] array
                );
            public static TVolume[] GetAllVolumesGUID()
            {
                TVolume[] volumes = new TVolume[10];
                int max = GetAllVolumesArrayGUID(10, volumes);
                return volumes;
            }


            [DllImport("DefragLibrary.dll", EntryPoint = "GetColouredBlocks",
            CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
            private static extern IntPtr GetColouredBlocksInternal([In] TVolume volume, UInt32 size);

            public static TQuad[] GetColouredBlocks(TVolume volume, uint size)
            {
                var quads = GetColouredBlocksInternal(volume, size);


                var blocks = new TQuad[size];
                for (int i = 0; i < size; i++)
                {
                    blocks[i] = Marshal.PtrToStructure<TQuad>(IntPtr.Add(quads, i * Marshal.SizeOf<TQuad>()));
                }

                return blocks;
            }

            [DllImport("DefragLibrary.dll", EntryPoint = "PartialDefr1",
            CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
            public static extern UInt64 DefragmentFirstStage([In] TVolume volume, UInt64 size);

            [DllImport("DefragLibrary.dll", EntryPoint = "InterruptDefrag",
             CallingConvention = CallingConvention.Cdecl)]
            public static extern void InterruptDefrag();

            [DllImport("DefragLibrary.dll", EntryPoint = "PartialDefr2",
            CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
            public static extern UInt64 DefragmentSecondStage([In] TVolume volume, UInt64 size);

        [DllImport("DefragLibrary.dll", EntryPoint = "GetVolumeSizeInBytes",
            CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        public static extern UInt64 GetVolumeSizeInBytes([In] TVolume volume);
        
    }
    //}
}

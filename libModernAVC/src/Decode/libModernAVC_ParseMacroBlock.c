#include "../../include/Private/Common/libModernAVC_Tables.h"
#include "../../include/Private/Common/libModernAVC_Types.h"
#include "../../include/Private/Common/libModernAVC_Common.h"
#include "../../include/Private/Decode/libModernAVC_Decode.h"
#include "../../include/Private/Decode/libModernAVC_ParseMacroBlock.h"
#include "../../include/Private/Decode/libModernAVC_ParseSlice.h"
#include "../../include/Private/Decode/libModernAVC_ParseNAL.h"

#ifdef __cplusplus
extern "C" {
#endif
    
    /*
     
     AVC ExpGolomb types.
     
     te(v) = Truncated, s
     ue(v) = Unsigned, MSBit, non-truncated
     
     */
    
    void ParseMacroBlockLayerInSVC(DecodeAVC *Dec, BitBuffer *BitB) { // macroblock_layer_in_scalable_extension
        if (Dec != NULL && BitB != NULL) {
            if (Dec->PPS->EntropyCodingMode == ExpGolomb) {
                if (InCropWindow(Dec, Dec->Slice->CurrentMacroBlockAddress) && (Dec->Slice->AdaptiveBaseModeFlag == true)) {
                    Dec->MacroBlock->BaseModeFlag = ReadBits(MSByteFirst, LSBitFirst, BitB, 1); // base_mode_flag
                }
                if (Dec->MacroBlock->BaseModeFlag == false) {
                    Dec->MacroBlock->Type = ReadExpGolomb(MSByteFirst, BitIOMSBit, BitB, CountUnary, AVCStopBit);
                }
                if (Dec->MacroBlock->Type == I_PCM) {
                    BitBuffer_Skip(BitB, 1); // pcm_alignment_zero_bit
                    CalculateMacroBlockDimensions(Dec);
                    for (uint16_t Sample = 0; Sample < MacroBlockMaxPixels; Sample++) {
                        Dec->MacroBlock->PCMLumaSample[Sample] = ReadBits(MSByteFirst, LSBitFirst, BitB, Dec->SPS->LumaBitDepthMinus8);
                    }
                    for (uint8_t Channel = 0; Channel < 2; Channel++) {
                        for (uint8_t Sample = 0; Sample < (2 * (Dec->SPS->MacroBlockWidthChroma * Dec->SPS->MacroBlockHeightChroma)); Sample++) {
                            Dec->MacroBlock->PCMChromaSample[Channel][Sample] = ReadBits(MSByteFirst, LSBitFirst, BitB, Dec->SPS->ChromaBitDepthMinus8);
                        }
                    }
                } else {
                    if (Dec->MacroBlock->BaseModeFlag == false) {
                        Dec->MacroBlock->NoMBSmallerThan8x8Flag = 1;
                        if ((Dec->MacroBlock->Type != I_NxN) && (MacroBlockPartitionPredictionMode(Dec, Dec->MacroBlock->Type, 0) != Intra_16x16) && NumMacroBlockPartitions(Dec->MacroBlock->Type) == 4) {
                            ParseSubMacroBlockPredictionInSVC(Dec, BitB);
                            for (uint8_t MacroBlockPiece = 0; MacroBlockPiece < 4; MacroBlockPiece++) {
                                if (Dec->MacroBlock->SubMacroBlockType[MacroBlockPiece] != B_Direct_8x8) {
                                    if (GetNumSubMacroBlockPartitions(Dec->MacroBlock->SubMacroBlockType[MacroBlockPiece]) > 1) {
                                        Dec->MacroBlock->NoMBSmallerThan8x8Flag = 0;
                                    }
                                } else if (Dec->SPS->Inference8x8 == false) {
                                    Dec->MacroBlock->NoMBSmallerThan8x8Flag = 0;
                                }
                            }
                        } else {
                            if (Dec->PPS->TransformIs8x8 == true && Dec->MacroBlock->Type == I_NxN) {
                                Dec->MacroBlock->TransformSizeIs8x8 = ReadBits(MSByteFirst, LSBitFirst, BitB, 1);
                            }
                            ParseMBPredictionInSVC(Dec, BitB);
                        }
                    }
                    if ((Dec->Slice->AdaptiveResidualPredictionFlag == true) && ((Dec->Slice->Type != SliceEI1) || (Dec->Slice->Type != SliceEI2)) && InCropWindow(Dec, Dec->Slice->CurrentMacroBlockAddress) && (Dec->MacroBlock->BaseModeFlag || (MacroBlockPartitionPredictionMode(Dec, Dec->MacroBlock->Type, 0) != Intra_16x16 && MacroBlockPartitionPredictionMode(Dec, Dec->MacroBlock->Type, 0) != Intra_8x8 && MacroBlockPartitionPredictionMode(Dec, Dec->MacroBlock->Type, 0) != Intra_4x4))) {
                        Dec->Slice->ResidualPredictionFlag = ReadBits(MSByteFirst, LSBitFirst, BitB, 1);
                    }
                    if (Dec->Slice->ScanIndexEnd >= Dec->Slice->ScanIndexStart) {
                        if (Dec->MacroBlock->BaseModeFlag || MacroBlockPartitionPredictionMode(Dec, Dec->MacroBlock->Type, 0) != Intra_16x16) {
                            Dec->MacroBlock->BlockPattern = ReadExpGolomb(MSByteFirst, BitIOMSBit, BitB, CountUnary, AVCStopBit); // MappedExp
                            if (Dec->MacroBlock->BlockPatternLuma > 0 && Dec->PPS->TransformIs8x8 && (Dec->MacroBlock->BaseModeFlag || (Dec->MacroBlock->Type != I_NxN && Dec->MacroBlock->NoMBSmallerThan8x8Flag && (Dec->MacroBlock->Type != B_Direct_16x16 || Dec->SPS->Inference8x8)))) {
                                Dec->MacroBlock->TransformSizeIs8x8 = ReadBits(MSByteFirst, LSBitFirst, BitB, 1);
                            }
                        }
                        if (Dec->MacroBlock->BlockPatternLuma > 0 || Dec->MacroBlock->BlockPatternChroma > 0 || MacroBlockPartitionPredictionMode(Dec, Dec->MacroBlock->Type, 0) == Intra_16x16) {
                            Dec->MacroBlock->QPDelta = ReadExpGolomb(MSByteFirst, BitIOMSBit, BitB, CountUnary, AVCStopBit);
                            residual(Dec->Slice->ScanIndexStart, Dec->Slice->ScanIndexEnd);
                        }
                    }
                }
            } else if (Dec->PPS->EntropyCodingMode == Arithmetic) {
                if (InCropWindow(Dec, Dec->Slice->CurrentMacroBlockAddress) && (Dec->Slice->AdaptiveBaseModeFlag == true)) {
                    Dec->MacroBlock->BaseModeFlag = ReadArithmetic(BitB, NULL, NULL, NULL, NULL);
                }
                if (Dec->MacroBlock->BaseModeFlag == false) {
                    Dec->MacroBlock->Type = ReadArithmetic(BitB, NULL, NULL, NULL, NULL);
                }
                if (Dec->MacroBlock->Type == I_PCM) {
                    BitBuffer_Skip(BitB, 1);
                    CalculateMacroBlockDimensions(Dec);
                    for (uint16_t Sample = 0; Sample < MacroBlockMaxPixels; Sample++) {
                        Dec->MacroBlock->PCMLumaSample[Sample] = ReadBits(MSByteFirst, LSBitFirst, BitB, Dec->SPS->LumaBitDepthMinus8);
                    }
                    for (uint8_t Channel = 0; Channel < 2; Channel++) {
                        for (uint8_t Sample = 0; Sample < (2 * (Dec->SPS->MacroBlockWidthChroma * Dec->SPS->MacroBlockHeightChroma)); Sample++) {
                            Dec->MacroBlock->PCMChromaSample[Channel][Sample] = ReadBits(MSByteFirst, LSBitFirst, BitB, Dec->SPS->ChromaBitDepthMinus8);
                        }
                    }
                } else {
                    if (Dec->MacroBlock->BaseModeFlag == false) {
                        Dec->MacroBlock->NoMBSmallerThan8x8Flag = 1;
                        if ((Dec->MacroBlock->Type != I_NxN) && (MacroBlockPartitionPredictionMode(Dec, Dec->MacroBlock->Type, 0) != Intra_16x16) && NumMacroBlockPartitions(Dec->MacroBlock->Type) == 4) {
                            ParseSubMacroBlockPredictionInSVC(Dec, BitB);
                            for (uint8_t MacroBlockPiece = 0; MacroBlockPiece < 4; MacroBlockPiece++) {
                                if (Dec->MacroBlock->SubMacroBlockType[MacroBlockPiece] != B_Direct_8x8) {
                                    if (GetNumSubMacroBlockPartitions(Dec->MacroBlock->SubMacroBlockType[MacroBlockPiece]) > 1) {
                                        Dec->MacroBlock->NoMBSmallerThan8x8Flag = 0;
                                    }
                                } else if (Dec->SPS->Inference8x8 == false) {
                                    Dec->MacroBlock->NoMBSmallerThan8x8Flag = 0;
                                }
                            }
                        } else {
                            if (Dec->PPS->TransformIs8x8 == true && Dec->MacroBlock->Type == I_NxN) {
                                Dec->MacroBlock->TransformSizeIs8x8 = ReadArithmetic(BitB, NULL, NULL, NULL, NULL);
                            }
                            ParseMBPredictionInSVC(Dec, BitB);
                        }
                    }
                    if ((Dec->Slice->AdaptiveResidualPredictionFlag == true) && ((Dec->Slice->Type != SliceEI1) || (Dec->Slice->Type != SliceEI2)) && InCropWindow(Dec, Dec->Slice->CurrentMacroBlockAddress) && (Dec->MacroBlock->BaseModeFlag || (MacroBlockPartitionPredictionMode(Dec, Dec->MacroBlock->Type, 0) != Intra_16x16 && MacroBlockPartitionPredictionMode(Dec, Dec->MacroBlock->Type, 0) != Intra_8x8 && MacroBlockPartitionPredictionMode(Dec, Dec->MacroBlock->Type, 0) != Intra_4x4))) {
                        Dec->Slice->ResidualPredictionFlag = Dec->PPS->EntropyCodingMode = ExpGolomb ? ReadExpGolomb(MSByteFirst, BitIOMSBit, BitB, CountUnary, AVCStopBit) : ReadArithmetic(BitB, NULL, NULL, NULL, NULL);
                    }
                    if (Dec->Slice->ScanIndexEnd >= Dec->Slice->ScanIndexStart) {
                        if (Dec->MacroBlock->BaseModeFlag || MacroBlockPartitionPredictionMode(Dec, Dec->MacroBlock->Type, 0) != Intra_16x16) {
                            Dec->MacroBlock->BlockPattern = ReadArithmetic(BitB, NULL, NULL, NULL, NULL); // ME or Arithmetic
                            if (Dec->MacroBlock->BlockPatternLuma > 0 && Dec->PPS->TransformIs8x8 && (Dec->MacroBlock->BaseModeFlag || (Dec->MacroBlock->Type != I_NxN && Dec->MacroBlock->NoMBSmallerThan8x8Flag && (Dec->MacroBlock->Type != B_Direct_16x16 || Dec->SPS->Inference8x8)))) {
                                Dec->MacroBlock->TransformSizeIs8x8 = ReadBits(MSByteFirst, LSBitFirst, BitB, 1); // OR Arithmetic
                            }
                        }
                        if (Dec->MacroBlock->BlockPatternLuma > 0 || Dec->MacroBlock->BlockPatternChroma > 0 || MacroBlockPartitionPredictionMode(Dec, Dec->MacroBlock->Type, 0) == Intra_16x16) {
                            Dec->MacroBlock->QPDelta = Dec->PPS->EntropyCodingMode = ExpGolomb ? ReadExpGolomb(MSByteFirst, LSBitFirst, BitB, true) : ReadArithmetic(BitB, NULL, NULL, NULL, NULL);
                            residual(Dec->Slice->ScanIndexStart, Dec->Slice->ScanIndexEnd);
                        }
                    }
                }
            }
        } else if (Dec == NULL) {
            Log(Log_ERROR, __func__, U8("DecodeAVC Pointer is NULL"));
        } else if (BitB == NULL) {
            Log(Log_ERROR, __func__, U8("BitBuffer Pointer is NULL"));
        }
    }
    
    void CalculateMacroBlockDimensions(DecodeAVC *Dec) {
        if (Dec != NULL && BitB != NULL) {
            if ((Dec->SPS->ChromaFormatIDC == ChromaBW) && (Dec->Slice->SeperateColorPlaneFlag == false)) {
                Dec->SPS->MacroBlockWidthChroma  = 0;
                Dec->SPS->MacroBlockHeightChroma = 0;
            } else if ((Dec->SPS->ChromaFormatIDC == Chroma420) && (Dec->Slice->SeperateColorPlaneFlag == false)) {
                Dec->SPS->MacroBlockWidthChroma  = 8;
                Dec->SPS->MacroBlockHeightChroma = 8;
            } else if ((Dec->SPS->ChromaFormatIDC == Chroma422) && (Dec->Slice->SeperateColorPlaneFlag == false)) {
                Dec->SPS->MacroBlockWidthChroma  = 8;
                Dec->SPS->MacroBlockHeightChroma = 16;
            } else if ((Dec->SPS->ChromaFormatIDC == Chroma444) && (Dec->Slice->SeperateColorPlaneFlag == false)) {
                Dec->SPS->MacroBlockWidthChroma  = 16;
                Dec->SPS->MacroBlockHeightChroma = 16;
            } else if (Dec->Slice->SeperateColorPlaneFlag == true) {
                // Samples need to be handled differently.
            }
        } else if (Dec == NULL) {
            Log(Log_ERROR, __func__, U8("DecodeAVC Pointer is NULL"));
        } else if (BitB == NULL) {
            Log(Log_ERROR, __func__, U8("BitBuffer Pointer is NULL"));
        }
        
    }
    
    void ParseMBPredictionInSVC(DecodeAVC *Dec, BitBuffer *BitB) { // mb_pred_in_scalable_extension
        if (Dec != NULL && BitB != NULL) {
            if (Dec->PPS->EntropyCodingMode == ExpGolomb) {
                if ((MacroBlockPartitionPredictionMode(Dec, Dec->MacroBlock->Type, 0) == Intra_4x4) || (MacroBlockPartitionPredictionMode(Dec, Dec->MacroBlock->Type, 0) == Intra_8x8) || (MacroBlockPartitionPredictionMode(Dec, Dec->MacroBlock->Type, 0) == Intra_16x16)) {
                    if (MacroBlockPartitionPredictionMode(Dec, Dec->MacroBlock->Type, 0) == Intra_4x4) {
                        for (uint8_t luma4x4BlkIdx = 0; luma4x4BlkIdx < 16; luma4x4BlkIdx++) {
                            Dec->MacroBlock->PreviousIntra4x4PredictionFlag[luma4x4BlkIdx] = ReadBits(MSByteFirst, LSBitFirst, BitB, 1);
                            if (!Dec->MacroBlock->PreviousIntra4x4PredictionFlag[luma4x4BlkIdx]) {
                                Dec->MacroBlock->RemIntra4x4PredictionMode[luma4x4BlkIdx] = ReadBits(MSByteFirst, LSBitFirst, BitB, 3);
                            }
                        }
                    }
                    if (MacroBlockPartitionPredictionMode(Dec, Dec->MacroBlock->Type, 0) == Intra_8x8) {
                        for (uint8_t luma8x8BlkIdx = 0; luma8x8BlkIdx < 4; luma8x8BlkIdx++) {
                            Dec->MacroBlock->PreviousIntra8x8PredictionFlag[luma8x8BlkIdx] = ReadBits(MSByteFirst, LSBitFirst, BitB, 1);
                            if (!Dec->MacroBlock->PreviousIntra8x8PredictionFlag[luma8x8BlkIdx]) {
                                Dec->MacroBlock->RemIntra8x8PredictionMode[luma8x8BlkIdx] = ReadBits(MSByteFirst, LSBitFirst, BitB, 3);
                            }
                        }
                    }
                    if (Dec->SPS->ChromaArrayType != 0) {
                        Dec->MacroBlock->IntraChromaPredictionMode = ReadExpGolomb(MSByteFirst, BitIOMSBit, BitB, CountUnary, AVCStopBit);
                    }
                } else if (MacroBlockPartitionPredictionMode(Dec, Dec->MacroBlock->Type, 0) != Direct) {
                    if (InCropWindow(Dec, Dec->Slice->CurrentMacroBlockAddress) && Dec->Slice->AdaptiveMotionPredictionFlag) {
                        for (uint32_t MacroBlockPiece = 0; MacroBlockPiece < NumMacroBlockPartitions(Dec->MacroBlock->Type); MacroBlockPiece++) {
                            if (MacroBlockPartitionPredictionMode(Dec, Dec->MacroBlock->Type, 0) != Pred_L1) {
                                Dec->MacroBlock->MotionPredictionFlagLevel0[MacroBlockPiece] = ReadBits(MSByteFirst, LSBitFirst, BitB, 1);
                            }
                            if (MacroBlockPartitionPredictionMode(Dec, Dec->MacroBlock->Type, 0) != Pred_L0) {
                                Dec->MacroBlock->MotionPredictionFlagLevel1[MacroBlockPiece] = ReadBits(MSByteFirst, LSBitFirst, BitB, 1);
                            }
                        }
                    }
                    for (uint32_t MacroBlockPiece = 0; MacroBlockPiece < NumMacroBlockPartitions(Dec->MacroBlock->Type); MacroBlockPiece++) {
                        if ((Dec->MacroBlock->NumRefIndexActiveLevel0 > 0 || Dec->Slice->MacroBlockFieldDecodingFlag != Dec->Slice->SliceIsInterlaced) && MacroBlockPartitionPredictionMode(Dec, Dec->MacroBlock->Type, 1) != Pred_L1 && !Dec->MacroBlock->MotionPredictionFlagLevel0[MacroBlockPiece]) {
                            Dec->MacroBlock->RefIndexLevel0[MacroBlockPiece] = ReadExpGolomb(MSByteFirst, LSBitFirst, BitB, true);
                        }
                        if ((Dec->MacroBlock->NumRefIndexActiveLevel1 > 0 || Dec->Slice->MacroBlockFieldDecodingFlag != Dec->Slice->SliceIsInterlaced) && MacroBlockPartitionPredictionMode(Dec, Dec->MacroBlock->Type, 1) != Pred_L0 && !Dec->MacroBlock->MotionPredictionFlagLevel1[MacroBlockPiece]) {
                            Dec->MacroBlock->RefIndexLevel1[MacroBlockPiece] = ReadExpGolomb(MSByteFirst, LSBitFirst, BitB, true);
                        }
                        if (MacroBlockPartitionPredictionMode(Dec, Dec->MacroBlock->Type, MacroBlockPiece) != Pred_L1) {
                            for (uint8_t compIdx = 0; compIdx < 2; compIdx++) {
                                Dec->MacroBlock->MVDLevel0[MacroBlockPiece][0][compIdx] = ReadExpGolomb(MSByteFirst, LSBitFirst, BitB, true);
                            }
                        }
                        if (MacroBlockPartitionPredictionMode(Dec, Dec->MacroBlock->Type, 1) != Pred_L0) {
                            for (uint8_t compIdx = 0; compIdx < 2; compIdx++) {
                                Dec->MacroBlock->MVDLevel1[MacroBlockPiece][0][compIdx] = ReadExpGolomb(MSByteFirst, LSBitFirst, BitB, true);
                            }
                        }
                    }
                }
            } else if (Dec->PPS->EntropyCodingMode == Arithmetic) {
                if ((MacroBlockPartitionPredictionMode(Dec, Dec->MacroBlock->Type, 0) == Intra_4x4) || (MacroBlockPartitionPredictionMode(Dec, Dec->MacroBlock->Type, 0) == Intra_8x8) || (MacroBlockPartitionPredictionMode(Dec, Dec->MacroBlock->Type, 0) == Intra_16x16)) {
                    if (MacroBlockPartitionPredictionMode(Dec, Dec->MacroBlock->Type, 0) == Intra_4x4) {
                        for (uint8_t luma4x4BlkIdx = 0; luma4x4BlkIdx < 16; luma4x4BlkIdx++) {
                            Dec->MacroBlock->PreviousIntra4x4PredictionFlag[luma4x4BlkIdx] = ReadArithmetic(BitB, NULL, NULL, NULL, NULL);
                            if (!Dec->MacroBlock->PreviousIntra4x4PredictionFlag[luma4x4BlkIdx]) {
                                Dec->MacroBlock->RemIntra4x4PredictionMode[luma4x4BlkIdx] = ReadArithmetic(BitB, NULL, NULL, NULL, NULL);
                            }
                        }
                    }
                    if (MacroBlockPartitionPredictionMode(Dec, Dec->MacroBlock->Type, 0) == Intra_8x8) {
                        for (uint8_t luma8x8BlkIdx = 0; luma8x8BlkIdx < 4; luma8x8BlkIdx++) {
                            Dec->MacroBlock->PreviousIntra8x8PredictionFlag[luma8x8BlkIdx] = ReadArithmetic(BitB, NULL, NULL, NULL, NULL);
                            if (Dec->MacroBlock->PreviousIntra8x8PredictionFlag[luma8x8BlkIdx] == false) {
                                Dec->MacroBlock->RemIntra8x8PredictionMode[luma8x8BlkIdx] = ReadArithmetic(BitB, NULL, NULL, NULL, NULL);
                            }
                        }
                    }
                    if (Dec->SPS->ChromaArrayType != 0) {
                        Dec->MacroBlock->IntraChromaPredictionMode = ReadArithmetic(BitB, NULL, NULL, NULL, NULL);
                    }
                } else if (MacroBlockPartitionPredictionMode(Dec, Dec->MacroBlock->Type, 0) != Direct) {
                    if (InCropWindow(Dec, Dec->Slice->CurrentMacroBlockAddress) && Dec->Slice->AdaptiveMotionPredictionFlag) {
                        for (uint32_t MacroBlockPiece = 0; MacroBlockPiece < NumMacroBlockPartitions(Dec->MacroBlock->Type); MacroBlockPiece++) {
                            if (MacroBlockPartitionPredictionMode(Dec, Dec->MacroBlock->Type, 1) != Pred_L1) {
                                Dec->MacroBlock->MotionPredictionFlagLevel0[MacroBlockPiece] = ReadArithmetic(BitB, NULL, NULL, NULL, NULL);
                            }
                            if (MacroBlockPartitionPredictionMode(Dec, Dec->MacroBlock->Type, 1) != Pred_L0) {
                                Dec->MacroBlock->MotionPredictionFlagLevel1[MacroBlockPiece] = ReadArithmetic(BitB, NULL, NULL, NULL, NULL);
                            }
                        }
                    }
                    for (uint32_t MacroBlockPiece = 0; MacroBlockPiece < NumMacroBlockPartitions(Dec->MacroBlock->Type); MacroBlockPiece++) {
                        if ((Dec->MacroBlock->NumRefIndexActiveLevel0 > 0 || Dec->Slice->MacroBlockFieldDecodingFlag != Dec->Slice->SliceIsInterlaced) && MacroBlockPartitionPredictionMode(Dec, Dec->MacroBlock->Type, 1) != Pred_L1 && !Dec->MacroBlock->MotionPredictionFlagLevel0[MacroBlockPiece]) {
                            Dec->MacroBlock->RefIndexLevel0[MacroBlockPiece] = ReadArithmetic(BitB, NULL, NULL, NULL, NULL);
                        }
                        if ((Dec->MacroBlock->NumRefIndexActiveLevel1 > 0 || Dec->Slice->MacroBlockFieldDecodingFlag != Dec->Slice->SliceIsInterlaced) &&
                            MacroBlockPartitionPredictionMode(Dec, Dec->MacroBlock->Type, 1) != Pred_L0 && !Dec->MacroBlock->MotionPredictionFlagLevel0[MacroBlockPiece]) {
                            Dec->MacroBlock->RefIndexLevel1[MacroBlockPiece] = ReadArithmetic(BitB, NULL, NULL, NULL, NULL);
                        }
                        if (MacroBlockPartitionPredictionMode(Dec, Dec->MacroBlock->Type, MacroBlockPiece) != Pred_L1) {
                            for (uint8_t compIdx = 0; compIdx < 2; compIdx++) {
                                Dec->MacroBlock->MVDLevel0[MacroBlockPiece][0][compIdx] = ReadArithmetic(BitB, NULL, NULL, NULL, NULL);
                            }
                        }
                        if (MacroBlockPartitionPredictionMode(Dec, Dec->MacroBlock->Type, 0) != Pred_L0) {
                            for (uint8_t compIdx = 0; compIdx < 2; compIdx++) {
                                Dec->MacroBlock->MVDLevel1[MacroBlockPiece][0][compIdx] = ReadArithmetic(BitB, NULL, NULL, NULL, NULL);
                            }
                        }
                    }
                }
            }
        } else if (Dec == NULL) {
            Log(Log_ERROR, __func__, U8("DecodeAVC Pointer is NULL"));
        } else if (BitB == NULL) {
            Log(Log_ERROR, __func__, U8("BitBuffer Pointer is NULL"));
        }
    }
    
    void ParseAVC3DMacroBlockPredictionExtension(DecodeAVC *Dec, BitBuffer *BitB) {
        if (Dec != NULL && BitB != NULL) {
            if (MacroBlockPartitionPredictionMode(Dec, Dec->MacroBlock->Type, 0) == Intra_4x4) {
                
            } else if (MacroBlockPartitionPredictionMode(Dec, Dec->MacroBlock->Type, 0) == Intra_8x8) {
                
            } else if (MacroBlockPartitionPredictionMode(Dec, Dec->MacroBlock->Type, 0) == Intra_16x16) {
                
            }
        } else if (Dec == NULL) {
            Log(Log_ERROR, __func__, U8("DecodeAVC Pointer is NULL"));
        } else if (BitB == NULL) {
            Log(Log_ERROR, __func__, U8("BitBuffer Pointer is NULL"));
        }
    }
    
    uint8_t NumMacroBlockPartitions(uint8_t MacroBlockType) { // NumMbPart
        if (Dec != NULL && BitB != NULL) {
            if (MacroBlockType == 0) {
                return 1;
            } else if ((MacroBlockType == 1) || (MacroBlockType == 2)) {
                return 2;
            } else if ((MacroBlockType == 3) || (MacroBlockType == 4)) {
                return 4;
            } else {
                return 1;
            }
        } else if (Dec == NULL) {
            Log(Log_ERROR, __func__, U8("DecodeAVC Pointer is NULL"));
        } else if (BitB == NULL) {
            Log(Log_ERROR, __func__, U8("BitBuffer Pointer is NULL"));
        }
    }
    
    void MacroBlockLayer(DecodeAVC *Dec, BitBuffer *BitB) { // macroblock_layer
        if (Dec != NULL && BitB != NULL) {
            if (Dec->PPS->EntropyCodingMode == ExpGolomb) {
                Dec->MacroBlock->Type = ReadExpGolomb(MSByteFirst, LSBitFirst, BitB, false);
                if (Dec->MacroBlock->Type == I_PCM) { // I_PCM
                    AlignBitBuffer(BitB, 1);
                    for (uint16_t i = 0; i < 256; i++) {
                        Dec->MacroBlock->PCMLumaSample[i] = ReadBits(MSByteFirst, LSBitFirst, BitB, Dec->SPS->LumaBitDepthMinus8);
                        for (uint8_t j = 0; j < 2; j++) { // FIXME: 2 needs to be a variable of the number of channels
                            Dec->MacroBlock->PCMChromaSample[j][i] = ReadBits(MSByteFirst, LSBitFirst, BitB, Dec->SPS->ChromaBitDepthMinus8);
                        }
                    }
                } else { // NOT I_PCM
                    Dec->MacroBlock->NoMBSmallerThan8x8Flag = true;
                    uint8_t NumMacroBlockPart = NumMacroBlockPartitions(Dec->MacroBlock->Type);
                    if ((Dec->MacroBlock->Type != I_NxN) && (MacroBlockPartitionPredictionMode(Dec, Dec->MacroBlock->Type, 0) != Intra_16x16) && (NumMacroBlockPart == 4)) {
                        ParseSubMacroBlockPrediction(Dec, BitB, Dec->MacroBlock->Type);
                        for (uint8_t MacroBlockPiece = 0; MacroBlockPiece < 4; MacroBlockPiece++) {
                            if (Dec->MacroBlock->SubMacroBlockType[MacroBlockPiece] != B_Direct_8x8) {
                                if (GetNumSubMacroBlockPartitions(Dec->MacroBlock->SubMacroBlockType[MacroBlockPiece]) > 1) {
                                    Dec->MacroBlock->NoMBSmallerThan8x8Flag = 0;
                                }
                            } else if (Dec->SPS->Inference8x8 == false) {
                                Dec->MacroBlock->NoMBSmallerThan8x8Flag = 0;
                            }
                        }
                    } else {
                        if (Dec->PPS->TransformIs8x8 && (Dec->MacroBlock->Type == I_NxN)) {
                            Dec->MacroBlock->TransformSizeIs8x8 = ReadBits(MSByteFirst, LSBitFirst, BitB, 1);
                        }
                        mb_pred(Dec, BitB, Dec->MacroBlock->Type);
                    }
                    if (MacroBlockPartitionPredictionMode(Dec, Dec->MacroBlock->Type, 0) != Intra_16x16) {
                        Dec->MacroBlock->BlockPattern = ReadExpGolomb(MSByteFirst, LSBitFirst, BitB, false); // TODO: Add table lookup
                        if (Dec->MacroBlock->BlockPattern == true && Dec->PPS->TransformIs8x8 && (Dec->MacroBlock->Type != I_NxN) && Dec->MacroBlock->NoMBSmallerThan8x8Flag && (Dec->MacroBlock->Type != B_Direct_16x16 || Dec->SPS->Inference8x8)) {
                            Dec->MacroBlock->TransformSizeIs8x8 = ReadBits(MSByteFirst, LSBitFirst, BitB, 1);
                        }
                        if (Dec->MacroBlock->BlockPatternLuma > 0 || Dec->MacroBlock->BlockPatternChroma > 0 || MacroBlockPartitionPredictionMode(Dec, Dec->MacroBlock->Type, 0) == Intra_16x16) {
                            Dec->MacroBlock->QPDelta = ReadExpGolomb(MSByteFirst, LSBitFirst, BitB, true);
                            residual(0, 15);
                        }
                    }
                }
            } else if (Dec->PPS->EntropyCodingMode == Arithmetic) {
                Dec->MacroBlock->Type = ReadArithmetic(BitB, NULL, NULL, NULL, NULL);
                if (Dec->MacroBlock->Type == I_PCM) { // I_PCM
                    AlignBitBuffer(BitB, 1);
                    for (uint16_t i = 0; i < 256; i++) {
                        Dec->MacroBlock->PCMLumaSample[i] = ReadBits(MSByteFirst, LSBitFirst, BitB, Dec->SPS->LumaBitDepthMinus8);
                        for (uint8_t j = 0; j < 2; j++) { // FIXME: 2 needs to be a variable of the number of channels
                            Dec->MacroBlock->PCMChromaSample[j][i] = ReadBits(MSByteFirst, LSBitFirst, BitB, Dec->SPS->ChromaBitDepthMinus8);
                        }
                    }
                } else { // NOT I_PCM
                    Dec->MacroBlock->NoMBSmallerThan8x8Flag = true;
                    uint8_t NumMacroBlockPart = NumMacroBlockPartitions(Dec->MacroBlock->Type);
                    if ((Dec->MacroBlock->Type != I_NxN) && (MacroBlockPartitionPredictionMode(Dec, Dec->MacroBlock->Type, 0) != Intra_16x16) && (NumMacroBlockPart == 4)) {
                        ParseSubMacroBlockPrediction(Dec, BitB, Dec->MacroBlock->Type);
                        for (uint8_t MacroBlockPiece = 0; MacroBlockPiece < 4; MacroBlockPiece++) {
                            if (Dec->MacroBlock->SubMacroBlockType[MacroBlockPiece] != B_Direct_8x8) {
                                if (GetNumSubMacroBlockPartitions(Dec->MacroBlock->SubMacroBlockType[MacroBlockPiece]) > 1) {
                                    Dec->MacroBlock->NoMBSmallerThan8x8Flag = 0;
                                }
                            } else if (Dec->SPS->Inference8x8 == false) {
                                Dec->MacroBlock->NoMBSmallerThan8x8Flag = 0;
                            }
                        }
                    } else {
                        if (Dec->PPS->TransformIs8x8 && (Dec->MacroBlock->Type == I_NxN)) {
                            Dec->MacroBlock->TransformSizeIs8x8 = ReadArithmetic(BitB, NULL, NULL, NULL, NULL);
                        }
                        mb_pred(Dec, BitB, Dec->MacroBlock->Type);
                    }
                    if (MacroBlockPartitionPredictionMode(Dec, Dec->MacroBlock->Type, 0) != Intra_16x16) {
                        Dec->MacroBlock->BlockPattern = ReadArithmetic(BitB, NULL, NULL, NULL, NULL);
                        if (Dec->MacroBlock->BlockPattern == true && Dec->PPS->TransformIs8x8 && (Dec->MacroBlock->Type != I_NxN) && Dec->MacroBlock->NoMBSmallerThan8x8Flag && (Dec->MacroBlock->Type != B_Direct_16x16 || Dec->SPS->Inference8x8)) {
                            Dec->MacroBlock->TransformSizeIs8x8 = ReadArithmetic(BitB, NULL, NULL, NULL, NULL);
                        }
                        if (Dec->MacroBlock->BlockPatternLuma > 0 || Dec->MacroBlock->BlockPatternChroma > 0 || MacroBlockPartitionPredictionMode(Dec, Dec->MacroBlock->Type, 0) == Intra_16x16) {
                            Dec->MacroBlock->QPDelta = ReadArithmetic(BitB, NULL, NULL, NULL, NULL);
                            residual(0, 15);
                        }
                    }
                }
            }
        } else if (Dec == NULL) {
            Log(Log_ERROR, __func__, U8("DecodeAVC Pointer is NULL"));
        } else if (BitB == NULL) {
            Log(Log_ERROR, __func__, U8("BitBuffer Pointer is NULL"));
        }
    }
    
    void mb_pred(DecodeAVC *Dec, BitBuffer *BitB, uint8_t mb_type) { // mb_pred
        if (Dec != NULL && BitB != NULL) {
            if (Dec->PPS->EntropyCodingMode == ExpGolomb) {
                uint8_t MacroBlockPredictionMode = MacroBlockPartitionPredictionMode(Dec, Dec->MacroBlock->Type, 0);
                if ((MacroBlockPredictionMode == Intra_4x4) || (MacroBlockPredictionMode == Intra_8x8) || (MacroBlockPredictionMode == Intra_16x16)) {
                    if (MacroBlockPredictionMode == Intra_4x4) {
                        for (uint8_t Luma4x4Block = 0; Luma4x4Block < 16; Luma4x4Block++) { // luma4x4BlkIdx
                            Dec->MacroBlock->PreviousIntra4x4PredictionFlag[Luma4x4Block] = ReadBits(MSByteFirst, LSBitFirst, BitB, 1);
                            if (Dec->MacroBlock->PreviousIntra4x4PredictionFlag[Luma4x4Block] == false) {
                                Dec->MacroBlock->RemIntra4x4PredictionMode[Luma4x4Block] = ReadBits(MSByteFirst, LSBitFirst, BitB, 3);
                            }
                        }
                    }
                    if (MacroBlockPredictionMode == Intra_8x8) {
                        for (uint8_t Luma8x8Block = 0; Luma8x8Block < 4; Luma8x8Block++) { // luma8x8BlkIdx
                            Dec->MacroBlock->PreviousIntra8x8PredictionFlag[Luma8x8Block] = ReadBits(MSByteFirst, LSBitFirst, BitB, 1);
                            if (Dec->MacroBlock->PreviousIntra8x8PredictionFlag[Luma8x8Block] == false) {
                                Dec->MacroBlock->RemIntra8x8PredictionMode[Luma8x8Block] = ReadBits(MSByteFirst, LSBitFirst, BitB, 3);
                            }
                        }
                    }
                    if ((Dec->SPS->ChromaArrayType == Chroma420) || (Dec->SPS->ChromaArrayType == Chroma422)) {
                        Dec->MacroBlock->IntraChromaPredictionMode = ReadExpGolomb(MSByteFirst, LSBitFirst, BitB, false);
                    }
                } else if (MacroBlockPredictionMode != Direct) {
                    for (uint8_t MacroBlockPiece = 0; MacroBlockPiece < NumMacroBlockPartitions(Dec->MacroBlock->Type); MacroBlockPiece++) {
                        if ((Dec->MacroBlock->NumRefIndexActiveLevel0 > 0 || Dec->Slice->MacroBlockFieldDecodingFlag != Dec->Slice->SliceIsInterlaced) && MacroBlockPartitionPredictionMode(Dec, Dec->MacroBlock->Type, 0) != Pred_L1) {
                            Dec->MacroBlock->RefIndexLevel0[MacroBlockPiece] = ReadRICE(BitB, true, 0);
                        }
                    }
                    for (uint32_t MacroBlockPiece = 0; MacroBlockPiece < NumMacroBlockPartitions(Dec->MacroBlock->Type); MacroBlockPiece++) {
                        if ((Dec->MacroBlock->NumRefIndexActiveLevel1 > 0 || Dec->Slice->MacroBlockFieldDecodingFlag != Dec->Slice->SliceIsInterlaced) && MacroBlockPartitionPredictionMode(Dec, Dec->MacroBlock->Type, 0) != Pred_L0) {
                            Dec->MacroBlock->RefIndexLevel1[MacroBlockPiece] = ReadRICE(BitB, true, 0);
                        }
                    }
                    for (uint32_t MacroBlockPiece = 0; MacroBlockPiece < NumMacroBlockPartitions(Dec->MacroBlock->Type); MacroBlockPiece++) {
                        if (MacroBlockPartitionPredictionMode(Dec, Dec->MacroBlock->Type, 0) != Pred_L1) {
                            for (uint8_t compIdx = 0; compIdx < 2; compIdx++) {
                                Dec->MacroBlock->MVDLevel0[MacroBlockPiece][0][compIdx] = ReadExpGolomb(MSByteFirst, LSBitFirst, BitB, true);
                            }
                        }
                    }
                    for (uint32_t MacroBlockPiece = 0; MacroBlockPiece < NumMacroBlockPartitions(Dec->MacroBlock->Type); MacroBlockPiece++) {
                        if (MacroBlockPartitionPredictionMode(Dec, Dec->MacroBlock->Type, 0) != Pred_L0) {
                            for (uint8_t compIdx = 0; compIdx < 2; compIdx++) {
                                Dec->MacroBlock->MVDLevel1[MacroBlockPiece][0][compIdx] = ReadExpGolomb(MSByteFirst, LSBitFirst, BitB, true);
                            }
                        }
                    }
                }
            } else if (Dec->PPS->EntropyCodingMode == Arithmetic) {
                uint8_t MacroBlockPredictionMode = MacroBlockPartitionPredictionMode(Dec, Dec->MacroBlock->Type, 0);
                if ((MacroBlockPredictionMode == Intra_4x4) || (MacroBlockPredictionMode == Intra_8x8) || (MacroBlockPredictionMode == Intra_16x16)) {
                    if (MacroBlockPredictionMode == Intra_4x4) {
                        for (uint8_t Luma4x4Block = 0; Luma4x4Block < 16; Luma4x4Block++) { // luma4x4BlkIdx
                            Dec->MacroBlock->PreviousIntra4x4PredictionFlag[Luma4x4Block] = ReadArithmetic(BitB, NULL, NULL, NULL, NULL);
                            if (Dec->MacroBlock->PreviousIntra4x4PredictionFlag[Luma4x4Block] == false) {
                                Dec->MacroBlock->RemIntra4x4PredictionMode[Luma4x4Block] = ReadArithmetic(BitB, NULL, NULL, NULL, NULL);
                            }
                        }
                    }
                    if (MacroBlockPredictionMode == Intra_8x8) {
                        for (uint8_t Luma8x8Block = 0; Luma8x8Block < 4; Luma8x8Block++) { // luma8x8BlkIdx
                            Dec->MacroBlock->PreviousIntra8x8PredictionFlag[Luma8x8Block] = ReadArithmetic(BitB, NULL, NULL, NULL, NULL);
                            if (Dec->MacroBlock->PreviousIntra8x8PredictionFlag[Luma8x8Block] == false) {
                                Dec->MacroBlock->RemIntra8x8PredictionMode[Luma8x8Block] = ReadArithmetic(BitB, NULL, NULL, NULL, NULL);
                            }
                        }
                    }
                    if ((Dec->SPS->ChromaArrayType == Chroma420) || (Dec->SPS->ChromaArrayType == Chroma422)) {
                        Dec->MacroBlock->IntraChromaPredictionMode = ReadArithmetic(BitB, NULL, NULL, NULL, NULL);
                    }
                } else if (MacroBlockPredictionMode != Direct) {
                    for (uint8_t MacroBlockPiece = 0; MacroBlockPiece < NumMacroBlockPartitions(Dec->MacroBlock->Type); MacroBlockPiece++) {
                        if ((Dec->MacroBlock->NumRefIndexActiveLevel0 > 0 || Dec->Slice->MacroBlockFieldDecodingFlag != Dec->Slice->SliceIsInterlaced) && MacroBlockPartitionPredictionMode(Dec, Dec->MacroBlock->Type, 0) != Pred_L1) {
                            Dec->MacroBlock->RefIndexLevel0[MacroBlockPiece] = ReadArithmetic(BitB, NULL, NULL, NULL, NULL);
                        }
                    }
                    for (uint32_t MacroBlockPiece = 0; MacroBlockPiece < NumMacroBlockPartitions(Dec->MacroBlock->Type); MacroBlockPiece++) {
                        if ((Dec->MacroBlock->NumRefIndexActiveLevel1 > 0 || Dec->Slice->MacroBlockFieldDecodingFlag != Dec->Slice->SliceIsInterlaced) && MacroBlockPartitionPredictionMode(Dec, Dec->MacroBlock->Type, 0) != Pred_L0) {
                            Dec->MacroBlock->RefIndexLevel1[MacroBlockPiece] = ReadArithmetic(BitB, NULL, NULL, NULL, NULL);
                        }
                    }
                    for (uint32_t MacroBlockPiece = 0; MacroBlockPiece < NumMacroBlockPartitions(Dec->MacroBlock->Type); MacroBlockPiece++) {
                        if (MacroBlockPartitionPredictionMode(Dec, Dec->MacroBlock->Type, 0) != Pred_L1) {
                            for (uint8_t compIdx = 0; compIdx < 2; compIdx++) {
                                Dec->MacroBlock->MVDLevel0[MacroBlockPiece][0][compIdx] = ReadArithmetic(BitB, NULL, NULL, NULL, NULL);
                            }
                        }
                    }
                    for (uint32_t MacroBlockPiece = 0; MacroBlockPiece < NumMacroBlockPartitions(Dec->MacroBlock->Type); MacroBlockPiece++) {
                        if (MacroBlockPartitionPredictionMode(Dec, Dec->MacroBlock->Type, 0) != Pred_L0) {
                            for (uint8_t compIdx = 0; compIdx < 2; compIdx++) {
                                Dec->MacroBlock->MVDLevel1[MacroBlockPiece][0][compIdx] = ReadArithmetic(BitB, NULL, NULL, NULL, NULL);
                            }
                        }
                    }
                }
            }
            Dec->Slice->PicHeightInMapUnits      = Dec->SPS->PicHeightInMapUnitsMinus1 + 1;
            Dec->Slice->FrameHeightInMacroBlocks = (2 - Dec->SPS->OnlyMacroBlocksInFrame) * Dec->Slice->PicHeightInMapUnits;
            Dec->Slice->PicWidthInMacroBlocks    = Dec->SPS->PicWidthInMacroBlocksMinus1 + 1;
            Dec->Slice->PicWidthInSamplesLuma    = Dec->Slice->PicWidthInMacroBlocks * 16;
            Dec->Slice->PicWidthInSamplesChroma  = Dec->Slice->PicWidthInMacroBlocks * Dec->SPS->MacroBlockWidthChroma;
            Dec->Slice->PicHeightInMacroBlocks   = Dec->Slice->FrameHeightInMacroBlocks / (Dec->Slice->SliceIsInterlaced + 1);
            Dec->Slice->RawMacroBlockSizeInBits  = 256 * Dec->SPS->BitDepthLuma + 2 * Dec->SPS->MacroBlockWidthChroma * Dec->SPS->MacroBlockHeightChroma * Dec->SPS->BitDepthChroma;
            Dec->SPS->QPBDOffsetChroma           = Dec->SPS->ChromaBitDepthMinus8 * 6;
        } else if (Dec == NULL) {
            Log(Log_ERROR, __func__, U8("DecodeAVC Pointer is NULL"));
        } else if (BitB == NULL) {
            Log(Log_ERROR, __func__, U8("BitBuffer Pointer is NULL"));
        }
    }
    
    void DecodeMacroBlock(DecodeAVC *Dec, uint8_t *MacroBlock, size_t MacroBlockSize) {
        if (Dec != NULL && BitB != NULL) {
            if (Dec->SPS->ChromaFormatIDC == ChromaBW) { // black and white
                
            } else if (Dec->SPS->ChromaFormatIDC == Chroma420) { // 4:2:0
                
            } else if (Dec->SPS->ChromaFormatIDC == Chroma422) { // 4:2:2
                
            } else if (Dec->SPS->ChromaFormatIDC == Chroma444) { // 4:4:4
                
            }
        } else if (Dec == NULL) {
            Log(Log_ERROR, __func__, U8("DecodeAVC Pointer is NULL"));
        } else if (BitB == NULL) {
            Log(Log_ERROR, __func__, U8("BitBuffer Pointer is NULL"));
        }
    }
    
    void ParseSubMacroBlockPredictionInSVC(DecodeAVC *Dec, BitBuffer *BitB) { // sub_mb_pred_in_scalable_extension
        if (Dec != NULL && BitB != NULL) {
            if (Dec->PPS->EntropyCodingMode == ExpGolomb) {
                for (uint8_t MacroBlockPiece = 0; MacroBlockPiece < 4; MacroBlockPiece++) { // MacroBlockPiece
                    Dec->MacroBlock->SubMacroBlockType[MacroBlockPiece] = ReadExpGolomb(MSByteFirst, LSBitFirst, BitB, CountUnary, AVCStopBit);
                }
                if ((InCropWindow(Dec, Dec->Slice->CurrentMacroBlockAddress) == true) && (Dec->Slice->AdaptiveMotionPredictionFlag == true)) {
                    for (uint8_t MacroBlockPiece = 0; MacroBlockPiece < 4; MacroBlockPiece++) {
                        if (SubMbPredMode(SubMacroBlockType[MacroBlockPiece]) != Direct && SubMbPredMode(SubMacroBlockType[MacroBlockPiece]) != Pred_L1) {
                            Dec->MacroBlock->MotionPredictionFlagLevel0[MacroBlockPiece] = ReadBits(MSByteFirst, LSBitFirst, BitB, 1);
                        }
                        if (SubMbPredMode(SubMacroBlockType[MacroBlockPiece]) != Direct && SubMbPredMode(SubMacroBlockType[MacroBlockPiece]) != Pred_L0) {
                            Dec->MacroBlock->MotionPredictionFlagLevel1[MacroBlockPiece] = ReadBits(MSByteFirst, LSBitFirst, BitB, 1);
                        }
                    }
                }
                for (uint8_t MacroBlockPiece = 0; MacroBlockPiece < 4; MacroBlockPiece++) { // MacroBlockPiece
                    if ((Dec->MacroBlock->NumRefIndexActiveLevel0 > 0 || Dec->Slice->MacroBlockFieldDecodingFlag != Dec->Slice->SliceIsInterlaced) && Dec->MacroBlock->Type != P_8x8ref0 && SubMacroBlockType[MacroBlockPiece] != B_Direct_8x8 && SubMbPredMode(SubMacroBlockType[MacroBlockPiece]) != Pred_L1 && !motion_prediction_flag_l0[MacroBlockPiece]) {
                        
                        Dec->SPS->RefIndexLevel0[MacroBlockPiece] = ReadRICE(BitB, true, 0) + 1;
                    }
                    if ((Dec->MacroBlock->NumRefIndexActiveLevel1 > 0 || Dec->Slice->MacroBlockFieldDecodingFlag != Dec->Slice->SliceIsInterlaced) && SubMacroBlockType[MacroBlockPiece] != B_Direct_8x8 && SubMbPredMode(SubMacroBlockType[MacroBlockPiece]) != Pred_L0 && !motion_prediction_flag_l1[MacroBlockPiece]) {
                        
                        Dec->SPS->RefIndexLevel1[MacroBlockPiece] = ReadRICE(BitB, true, 0) + 1;
                        
                    }
                    
                    if (SubMacroBlockType[MacroBlockPiece] != B_Direct_8x8 && SubMbPredMode(SubMacroBlockType[MacroBlockPiece]) != Pred_L1) {
                        for (uint32_t SubMacroBlockPiece = 0; SubMacroBlockPiece < NumSubMbPart(SubMacroBlockType[MacroBlockPiece]); SubMacroBlockPiece++) {
                            for (uint8_t compIdx = 0; compIdx < 2; compIdx++) {
                                Dec->MacroBlock->MVDLevel0[MacroBlockPiece][SubMacroBlockPiece][compIdx] = ReadExpGolomb(MSByteFirst, LSBitFirst, BitB, WholeUnary, AVCStopBit);
                            }
                        }
                    }
                    
                    if (SubMacroBlockType[MacroBlockPiece] != B_Direct_8x8 && SubMbPredMode(SubMacroBlockType[MacroBlockPiece]) != Pred_L0) {
                        for (uint32_t SubMacroBlockPiece = 0; SubMacroBlockPiece < NumSubMbPart(SubMacroBlockType[MacroBlockPiece]); SubMacroBlockPiece++) {
                            for (uint8_t compIdx = 0; compIdx < 2; compIdx++) {
                                Dec->MacroBlock->MVDLevel1[MacroBlockPiece][SubMacroBlockPiece][compIdx] = ReadExpGolomb(MSByteFirst, LSBitFirst, BitB, WholeUnary, AVCStopBit);
                            }
                        }
                    }
                }
            } else if (Dec->PPS->EntropyCodingMode == Arithmetic) {
                for (uint8_t MacroBlockPiece = 0; MacroBlockPiece < 4; MacroBlockPiece++) { // MacroBlockPiece
                    Dec->MacroBlock->SubMacroBlockType[MacroBlockPiece] = ReadArithmetic(BitB, NULL, NULL, NULL, NULL);
                }
                if ((InCropWindow(Dec, Dec->Slice->CurrentMacroBlockAddress) == true) && (Dec->Slice->AdaptiveMotionPredictionFlag == true)) {
                    for (uint8_t MacroBlockPiece = 0; MacroBlockPiece < 4; MacroBlockPiece++) {
                        if (SubMbPredMode(SubMacroBlockType[MacroBlockPiece]) != Direct && SubMbPredMode(SubMacroBlockType[MacroBlockPiece]) != Pred_L1) {
                            Dec->MacroBlock->MotionPredictionFlagLevel0[MacroBlockPiece] = ReadArithmetic(BitB, NULL, NULL, NULL, NULL);
                        }
                        if (SubMbPredMode(SubMacroBlockType[MacroBlockPiece]) != Direct && SubMbPredMode(SubMacroBlockType[MacroBlockPiece]) != Pred_L0) {
                            Dec->MacroBlock->MotionPredictionFlagLevel1[MacroBlockPiece] = ReadArithmetic(BitB, NULL, NULL, NULL, NULL);
                        }
                    }
                }
                for (uint8_t MacroBlockPiece = 0; MacroBlockPiece < 4; MacroBlockPiece++) { // MacroBlockPiece
                    if ((Dec->MacroBlock->NumRefIndexActiveLevel0 > 0 || Dec->Slice->MacroBlockFieldDecodingFlag != Dec->Slice->SliceIsInterlaced) && Dec->MacroBlock->Type != P_8x8ref0 && SubMacroBlockType[MacroBlockPiece] != B_Direct_8x8 && SubMbPredMode(SubMacroBlockType[MacroBlockPiece]) != Pred_L1 && !motion_prediction_flag_l0[MacroBlockPiece]) {
                        
                        Dec->SPS->RefIndexLevel0[MacroBlockPiece] = ReadArithmetic(BitB, NULL, NULL, NULL, NULL) + 1;
                        
                    }
                    if ((Dec->MacroBlock->NumRefIndexActiveLevel1 > 0 || Dec->Slice->MacroBlockFieldDecodingFlag != Dec->Slice->SliceIsInterlaced) && SubMacroBlockType[MacroBlockPiece] != B_Direct_8x8 && SubMbPredMode(SubMacroBlockType[MacroBlockPiece]) != Pred_L0 && !motion_prediction_flag_l1[MacroBlockPiece]) {
                        
                        Dec->SPS->RefIndexLevel1[MacroBlockPiece] = ReadArithmetic(BitB, NULL, NULL, NULL, NULL) + 1;
                        
                    }
                    
                    if (SubMacroBlockType[MacroBlockPiece] != B_Direct_8x8 && SubMbPredMode(SubMacroBlockType[MacroBlockPiece]) != Pred_L1) {
                        for (uint32_t SubMacroBlockPiece = 0; SubMacroBlockPiece < NumSubMbPart(SubMacroBlockType[MacroBlockPiece]); SubMacroBlockPiece++) {
                            for (uint8_t compIdx = 0; compIdx < 2; compIdx++) {
                                Dec->MacroBlock->MVDLevel0[MacroBlockPiece][SubMacroBlockPiece][compIdx] = ReadArithmetic(BitB, NULL, NULL, NULL, NULL);
                            }
                        }
                    }
                    
                    if (SubMacroBlockType[MacroBlockPiece] != B_Direct_8x8 && SubMbPredMode(SubMacroBlockType[MacroBlockPiece]) != Pred_L0) {
                        for (uint32_t SubMacroBlockPiece = 0; SubMacroBlockPiece < NumSubMbPart(SubMacroBlockType[MacroBlockPiece]); SubMacroBlockPiece++) {
                            for (uint8_t compIdx = 0; compIdx < 2; compIdx++) {
                                Dec->MacroBlock->MVDLevel1[MacroBlockPiece][SubMacroBlockPiece][compIdx] = ReadArithmetic(BitB, NULL, NULL, NULL, NULL);
                            }
                        }
                    }
                }
            }
        } else if (Dec == NULL) {
            Log(Log_ERROR, __func__, U8("DecodeAVC Pointer is NULL"));
        } else if (BitB == NULL) {
            Log(Log_ERROR, __func__, U8("BitBuffer Pointer is NULL"));
        }
    }
    
    void ParseReferenceBasePictureSyntax(DecodeAVC *Dec, BitBuffer *BitB) { // dec_ref_base_pic_marking
        if (Dec != NULL && BitB != NULL) {
            Dec->SVC->AdaptiveMarkingModeFlag = ReadBits(MSByteFirst, LSBitFirst, BitB, 1);
            if (Dec->SVC->AdaptiveMarkingModeFlag == true) {
                Dec->SVC->BaseControlOperation = ReadExpGolomb(MSByteFirst, LSBitFirst, BitB, CountUnary, AVCStopBit);
                if (Dec->SVC->BaseControlOperation == 1) {
                    Dec->SVC->NumBasePicDiff = ReadExpGolomb(MSByteFirst, LSBitFirst, BitB, CountUnary, AVCStopBit) + 1;
                } else if (Dec->SVC->BaseControlOperation == 2) {
                    Dec->SVC->LongTermBasePicNum = ReadExpGolomb(MSByteFirst, LSBitFirst, BitB, CountUnary, AVCStopBit);
                } while(Dec->SVC->BaseControlOperation != 0) {
                    
                }
            }
        } else if (Dec == NULL) {
            Log(Log_ERROR, __func__, U8("DecodeAVC Pointer is NULL"));
        } else if (BitB == NULL) {
            Log(Log_ERROR, __func__, U8("BitBuffer Pointer is NULL"));
        }
    }
    
    void ParseSubMacroBlockPrediction(DecodeAVC *Dec, BitBuffer *BitB, uint8_t mb_type) { // sub_mb_pred
        if (Dec != NULL && BitB != NULL) {
            if (Dec->PPS->EntropyCodingMode == ExpGolomb) {
                for (uint8_t MacroBlockPiece = 0; MacroBlockPiece < 4; MacroBlockPiece++) {
                    SubMacroBlockType[MacroBlockPiece] = ReadExpGolomb(MSByteFirst, LSBitFirst, BitB, CountUnary, AVCStopBit);
                }
                for (uint8_t MacroBlockPiece = 0; MacroBlockPiece < 4; MacroBlockPiece++) {
                    if ((Dec->MacroBlock->NumRefIndexActiveLevel0 > 0 || Dec->Slice->MacroBlockFieldDecodingFlag != Dec->Slice->SliceIsInterlaced) && Dec->MacroBlock->Type != P_8x8ref0 && SubMacroBlockType[MacroBlockPiece] != B_Direct_8x8 && SubMbPredMode(SubMacroBlockType[MacroBlockPiece]) != Pred_L1) {
                        Dec->MacroBlock->RefIndexLevel0[MacroBlockPiece] = ReadExpGolomb(MSByteFirst, LSBitFirst, BitB, true) + 1;
                    }
                }
                for (uint8_t MacroBlockPiece = 0; MacroBlockPiece < 4; MacroBlockPiece++) {
                    if ((Dec->MacroBlock->NumRefIndexActiveLevel1 > 0 || Dec->Slice->MacroBlockFieldDecodingFlag != Dec->Slice->SliceIsInterlaced) && SubMacroBlockType[MacroBlockPiece] != B_Direct_8x8 && SubMbPredMode(SubMacroBlockType[MacroBlockPiece]) != Pred_L0) {
                        Dec->MacroBlock->RefIndexLevel1[MacroBlockPiece] = ReadExpGolomb(MSByteFirst, LSBitFirst, BitB, true) + 1;
                    }
                }
                for (uint8_t MacroBlockPiece = 0; MacroBlockPiece < 4; MacroBlockPiece++) {
                    if (SubMacroBlockType[MacroBlockPiece] != B_Direct_8x8 && SubMbPredMode(SubMacroBlockType[MacroBlockPiece]) !=Pred_L1) {
                        for (uint32_t SubMacroBlockPiece = 0; SubMacroBlockPiece < NumSubMbPart(SubMacroBlockType[MacroBlockPiece]);SubMacroBlockPiece++) {
                            for (uint8_t compIdx = 0; compIdx < 2; compIdx++) {
                                Dec->MacroBlock->MVDLevel0[MacroBlockPiece][SubMacroBlockPiece][compIdx] = ReadExpGolomb(MSByteFirst, LSBitFirst, BitB, true);
                            }
                        }
                    }
                }
                for (uint8_t MacroBlockPiece = 0; MacroBlockPiece < 4; MacroBlockPiece++) {
                    if (SubMacroBlockType[MacroBlockPiece] != B_Direct_8x8 && SubMbPredMode(SubMacroBlockType[MacroBlockPiece]) != Pred_L0) {
                        for (uint32_t SubMacroBlockPiece = 0; SubMacroBlockPiece < NumSubMbPart(SubMacroBlockType[MacroBlockPiece]);SubMacroBlockPiece++) {
                            for (uint8_t compIdx = 0; compIdx < 2; compIdx++) {
                                Dec->MacroBlock->MVDLevel1[MacroBlockPiece][SubMacroBlockPiece][compIdx] = ReadExpGolomb(MSByteFirst, LSBitFirst, BitB, true);
                            }
                        }
                    }
                }
            } else if (Dec->PPS->EntropyCodingMode == Arithmetic) {
                for (uint8_t MacroBlockPiece = 0; MacroBlockPiece < 4; MacroBlockPiece++) {
                    SubMacroBlockType[MacroBlockPiece] = ReadArithmetic(BitB, NULL, NULL, NULL, NULL);
                }
                for (uint8_t MacroBlockPiece = 0; MacroBlockPiece < 4; MacroBlockPiece++) {
                    if ((Dec->MacroBlock->NumRefIndexActiveLevel0 > 0 || Dec->Slice->MacroBlockFieldDecodingFlag != Dec->Slice->SliceIsInterlaced) && Dec->MacroBlock->Type != P_8x8ref0 && SubMacroBlockType[MacroBlockPiece] != B_Direct_8x8 && SubMbPredMode(SubMacroBlockType[MacroBlockPiece]) != Pred_L1) {
                        Dec->MacroBlock->RefIndexLevel0[MacroBlockPiece] = ReadArithmetic(BitB, NULL, NULL, NULL, NULL) + 1;
                    }
                }
                for (uint8_t MacroBlockPiece = 0; MacroBlockPiece < 4; MacroBlockPiece++) {
                    if ((Dec->MacroBlock->NumRefIndexActiveLevel1 > 0 || Dec->Slice->MacroBlockFieldDecodingFlag != Dec->Slice->SliceIsInterlaced) && SubMacroBlockType[MacroBlockPiece] != B_Direct_8x8 && SubMbPredMode(SubMacroBlockType[MacroBlockPiece]) != Pred_L0) {
                        Dec->MacroBlock->RefIndexLevel1[MacroBlockPiece] = ReadArithmetic(BitB, NULL, NULL, NULL, NULL) + 1;
                    }
                }
                for (uint8_t MacroBlockPiece = 0; MacroBlockPiece < 4; MacroBlockPiece++) {
                    if (SubMacroBlockType[MacroBlockPiece] != B_Direct_8x8 && SubMbPredMode(SubMacroBlockType[MacroBlockPiece]) !=Pred_L1) {
                        for (uint32_t SubMacroBlockPiece = 0; SubMacroBlockPiece < NumSubMbPart(SubMacroBlockType[MacroBlockPiece]);SubMacroBlockPiece++) {
                            for (uint8_t compIdx = 0; compIdx < 2; compIdx++) {
                                Dec->MacroBlock->MVDLevel0[MacroBlockPiece][SubMacroBlockPiece][compIdx] = ReadArithmetic(BitB, NULL, NULL, NULL, NULL);
                            }
                        }
                    }
                }
                for (uint8_t MacroBlockPiece = 0; MacroBlockPiece < 4; MacroBlockPiece++) {
                    if (SubMacroBlockType[MacroBlockPiece] != B_Direct_8x8 && SubMbPredMode(SubMacroBlockType[MacroBlockPiece]) != Pred_L0) {
                        for (uint32_t SubMacroBlockPiece = 0; SubMacroBlockPiece < NumSubMbPart(SubMacroBlockType[MacroBlockPiece]);SubMacroBlockPiece++) {
                            for (uint8_t compIdx = 0; compIdx < 2; compIdx++) {
                                Dec->MacroBlock->MVDLevel1[MacroBlockPiece][SubMacroBlockPiece][compIdx] = ReadArithmetic(BitB, NULL, NULL, NULL, NULL);
                            }
                        }
                    }
                }
            }
        } else if (Dec == NULL) {
            Log(Log_ERROR, __func__, U8("DecodeAVC Pointer is NULL"));
        } else if (BitB == NULL) {
            Log(Log_ERROR, __func__, U8("BitBuffer Pointer is NULL"));
        }
    }
    
#ifdef __cplusplus
}
#endif

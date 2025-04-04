/*
 * Copyright (c) 2021, 2024, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the MIT License.
 */
package ai.onnxruntime;

import static org.junit.jupiter.api.Assertions.assertArrayEquals;
import static org.junit.jupiter.api.Assertions.assertFalse;
import static org.junit.jupiter.api.Assertions.assertThrows;

import ai.onnxruntime.platform.Fp16Conversions;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.FloatBuffer;
import java.nio.IntBuffer;
import java.nio.ShortBuffer;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.SplittableRandom;
import org.junit.jupiter.api.Assertions;
import org.junit.jupiter.api.Test;

public class OnnxTensorTest {
  private static final OrtEnvironment env = TestHelpers.getOrtEnvironment();

  @Test
  public void testScalarCreation() throws OrtException {
    String[] stringValues = new String[] {"true", "false"};
    for (String s : stringValues) {
      try (OnnxTensor t = OnnxTensor.createTensor(env, s)) {
        Assertions.assertEquals(s, t.getValue());
      }
    }

    boolean[] boolValues = new boolean[] {true, false};
    for (boolean b : boolValues) {
      try (OnnxTensor t = OnnxTensor.createTensor(env, b)) {
        Assertions.assertEquals(b, t.getValue());
      }
    }

    int[] intValues =
        new int[] {-1, 0, 1, 12345678, -12345678, Integer.MAX_VALUE, Integer.MIN_VALUE};
    for (int i : intValues) {
      try (OnnxTensor t = OnnxTensor.createTensor(env, i)) {
        Assertions.assertEquals(i, t.getValue());
      }
    }

    long[] longValues =
        new long[] {-1L, 0L, 1L, 12345678L, -12345678L, Long.MAX_VALUE, Long.MIN_VALUE};
    for (long l : longValues) {
      try (OnnxTensor t = OnnxTensor.createTensor(env, l)) {
        Assertions.assertEquals(l, t.getValue());
      }
    }

    float[] floatValues =
        new float[] {
          -1.0f,
          0.0f,
          -0.0f,
          1.0f,
          1234.5678f,
          -1234.5678f,
          (float) Math.PI,
          (float) Math.E,
          Float.MAX_VALUE,
          Float.MIN_VALUE
        };
    for (float f : floatValues) {
      try (OnnxTensor t = OnnxTensor.createTensor(env, f)) {
        Assertions.assertEquals(f, t.getValue());
      }
    }

    double[] doubleValues =
        new double[] {
          -1.0,
          0.0,
          -0.0,
          1.0,
          1234.5678,
          -1234.5678,
          Math.PI,
          Math.E,
          Double.MAX_VALUE,
          Double.MIN_VALUE
        };
    for (double d : doubleValues) {
      try (OnnxTensor t = OnnxTensor.createTensor(env, d)) {
        Assertions.assertEquals(d, t.getValue());
      }
    }
  }

  @Test
  public void testArrayCreation() throws OrtException {
    // Test creating a value from a single dimensional array
    float[] arrValues = new float[] {0, 1, 2, 3, 4};
    try (OnnxTensor t = OnnxTensor.createTensor(env, arrValues)) {
      Assertions.assertTrue(t.ownsBuffer());
      Assertions.assertTrue(t.getBufferRef().isPresent());
      FloatBuffer buf = (FloatBuffer) t.getBufferRef().get();
      float[] output = new float[arrValues.length];
      buf.get(output);
      Assertions.assertArrayEquals(arrValues, output);

      // Can modify the tensor through this buffer.
      buf.put(0, 25);
      Assertions.assertArrayEquals(new float[] {25, 1, 2, 3, 4}, (float[]) t.getValue());
    }

    // Test creating a value from a multidimensional float array
    float[][][] arr3dValues =
        new float[][][] {
          {{0, 1, 2}, {3, 4, 5}},
          {{6, 7, 8}, {9, 10, 11}},
          {{12, 13, 14}, {15, 16, 17}},
          {{18, 19, 20}, {21, 22, 23}}
        };
    try (OnnxTensor t = OnnxTensor.createTensor(env, arr3dValues)) {
      Assertions.assertArrayEquals(new long[] {4, 2, 3}, t.getInfo().getShape());
      Assertions.assertTrue(t.ownsBuffer());
      Assertions.assertTrue(t.getBufferRef().isPresent());
      float[][][] output = (float[][][]) t.getValue();
      Assertions.assertArrayEquals(arr3dValues, output);

      // Can modify the tensor through the buffer.
      FloatBuffer buf = (FloatBuffer) t.getBufferRef().get();
      buf.put(0, 25);
      buf.put(12, 32);
      buf.put(13, 33);
      buf.put(23, 35);
      arr3dValues[0][0][0] = 25;
      arr3dValues[2][0][0] = 32;
      arr3dValues[2][0][1] = 33;
      arr3dValues[3][1][2] = 35;
      output = (float[][][]) t.getValue();
      Assertions.assertArrayEquals(arr3dValues, output);
    }

    // Test creating a value from a multidimensional int array
    int[][][] iArr3dValues =
        new int[][][] {
          {{0, 1, 2}, {3, 4, 5}},
          {{6, 7, 8}, {9, 10, 11}},
          {{12, 13, 14}, {15, 16, 17}},
          {{18, 19, 20}, {21, 22, 23}}
        };
    try (OnnxTensor t = OnnxTensor.createTensor(env, iArr3dValues)) {
      Assertions.assertArrayEquals(new long[] {4, 2, 3}, t.getInfo().getShape());
      Assertions.assertTrue(t.ownsBuffer());
      Assertions.assertTrue(t.getBufferRef().isPresent());
      int[][][] output = (int[][][]) t.getValue();
      Assertions.assertArrayEquals(iArr3dValues, output);

      // Can modify the tensor through the buffer.
      IntBuffer buf = (IntBuffer) t.getBufferRef().get();
      buf.put(0, 25);
      iArr3dValues[0][0][0] = 25;
      output = (int[][][]) t.getValue();
      Assertions.assertArrayEquals(iArr3dValues, output);
    }

    // Test creating a value from a ragged array throws
    int[][][] ragged =
        new int[][][] {
          {{0, 1, 2}, {3, 4, 5}},
          {{6, 7, 8}},
          {{12, 13}, {15, 16, 17}},
          {{18, 19, 20}, {21, 22, 23}}
        };
    try (OnnxTensor t = OnnxTensor.createTensor(env, ragged)) {
      Assertions.fail("Can't create tensors from ragged arrays");
    } catch (OrtException e) {
      Assertions.assertTrue(e.getMessage().contains("ragged"));
    }

    // Test creating a value from a non-array, non-primitive type throws.
    List<Integer> list = new ArrayList<>(5);
    list.add(5);
    try (OnnxTensor t = OnnxTensor.createTensor(env, list)) {
      Assertions.fail("Can't create tensors from lists");
    } catch (OrtException e) {
      Assertions.assertTrue(e.getMessage().contains("Cannot convert"));
    }
  }

  @Test
  public void testBufferCreation() throws OrtException {
    // Test creating a value from a non-direct byte buffer
    // Non-direct byte buffers are allocated on the Java heap and must be copied into off-heap
    // direct byte buffers which can be directly passed to ORT
    float[] arrValues = new float[] {0, 1, 2, 3, 4};
    FloatBuffer nonDirectBuffer = FloatBuffer.allocate(5);
    nonDirectBuffer.put(arrValues);
    nonDirectBuffer.rewind();
    try (OnnxTensor t = OnnxTensor.createTensor(env, nonDirectBuffer, new long[] {1, 5})) {
      // non-direct buffers trigger a copy
      Assertions.assertTrue(t.ownsBuffer());
      // tensors backed by buffers can get the buffer ref back out
      Assertions.assertTrue(t.getBufferRef().isPresent());
      FloatBuffer buf = t.getFloatBuffer();
      float[] output = new float[arrValues.length];
      buf.get(output);
      Assertions.assertArrayEquals(arrValues, output);

      // Can't modify the tensor through getFloatBuffer.
      buf.put(0, 25);
      Assertions.assertArrayEquals(arrValues, output);

      // Can modify the tensor through getBufferRef.
      FloatBuffer ref = (FloatBuffer) t.getBufferRef().get();
      ref.put(0, 25);
      buf = t.getFloatBuffer();
      buf.get(output);
      Assertions.assertEquals(25, output[0]);
    }

    // Test creating a value from a direct byte buffer
    // Direct byte buffers can be passed into ORT without additional copies or processing
    FloatBuffer directBuffer =
        ByteBuffer.allocateDirect(5 * 4).order(ByteOrder.nativeOrder()).asFloatBuffer();
    directBuffer.put(arrValues);
    directBuffer.rewind();
    try (OnnxTensor t = OnnxTensor.createTensor(env, directBuffer, new long[] {1, 5})) {
      // direct buffers don't trigger a copy
      assertFalse(t.ownsBuffer());
      // tensors backed by buffers can get the buffer ref back out
      Assertions.assertTrue(t.getBufferRef().isPresent());
      FloatBuffer buf = t.getFloatBuffer();
      float[] output = new float[arrValues.length];
      buf.get(output);
      Assertions.assertArrayEquals(arrValues, output);

      // Can't modify the tensor through getFloatBuffer.
      buf.put(0, 25);
      Assertions.assertArrayEquals(arrValues, output);

      // Can modify the tensor through getBufferRef.
      FloatBuffer ref = (FloatBuffer) t.getBufferRef().get();
      ref.put(0, 25);
      buf = t.getFloatBuffer();
      buf.get(output);
      Assertions.assertEquals(25, output[0]);

      // Can modify the tensor through our original ref to the direct byte buffer
      directBuffer.put(1, 15);
      buf = t.getFloatBuffer();
      buf.get(output);
      Assertions.assertEquals(15, output[1]);
    }
  }

  @Test
  public void testStringCreation() throws OrtException {
    String[] arrValues = new String[] {"this", "is", "a", "single", "dimensional", "string"};
    try (OnnxTensor t = OnnxTensor.createTensor(env, arrValues)) {
      Assertions.assertArrayEquals(new long[] {6}, t.getInfo().shape);
      String[] output = (String[]) t.getValue();
      Assertions.assertArrayEquals(arrValues, output);
    }

    String[][] stringValues =
        new String[][] {{"this", "is", "a"}, {"multi", "dimensional", "string"}};
    try (OnnxTensor t = OnnxTensor.createTensor(env, stringValues)) {
      Assertions.assertArrayEquals(new long[] {2, 3}, t.getInfo().shape);
      String[][] output = (String[][]) t.getValue();
      Assertions.assertArrayEquals(stringValues, output);
    }

    String[][][] deepStringValues =
        new String[][][] {
          {{"this", "is", "a"}, {"multi", "dimensional", "string"}},
          {{"with", "lots", "more"}, {"dimensions", "than", "before"}}
        };
    try (OnnxTensor t = OnnxTensor.createTensor(env, deepStringValues)) {
      Assertions.assertArrayEquals(new long[] {2, 2, 3}, t.getInfo().shape);
      String[][][] output = (String[][][]) t.getValue();
      Assertions.assertArrayEquals(deepStringValues, output);
    }
  }

  @Test
  public void testUint8Creation() throws OrtException {
    byte[] buf = new byte[] {0, 1};
    ByteBuffer data = ByteBuffer.wrap(buf);
    long[] shape = new long[] {2};
    try (OnnxTensor t = OnnxTensor.createTensor(env, data, shape, OnnxJavaType.UINT8)) {
      Assertions.assertArrayEquals(buf, (byte[]) t.getValue());
    }
  }

  @Test
  public void testByteBufferCreation() throws OrtException {
    ByteBuffer byteBuf = ByteBuffer.allocateDirect(Float.BYTES * 5).order(ByteOrder.nativeOrder());
    FloatBuffer floatBuf = byteBuf.asFloatBuffer();
    floatBuf.put(1.0f);
    floatBuf.put(2.0f);
    floatBuf.put(3.0f);
    floatBuf.put(4.0f);
    floatBuf.put(5.0f);
    floatBuf.position(1);
    float[] expected = new float[floatBuf.remaining()];
    floatBuf.get(expected);
    floatBuf.position(1);
    byteBuf.position(4);
    try (OnnxTensor t =
        OnnxTensor.createTensor(
            env, byteBuf, new long[] {floatBuf.remaining()}, OnnxJavaType.FLOAT)) {
      Assertions.assertNotNull(t);
      float[] actual = (float[]) t.getValue();
      Assertions.assertArrayEquals(expected, actual);
    }
  }

  @Test
  public void testEmptyTensor() throws OrtException {
    FloatBuffer buf = FloatBuffer.allocate(0);
    long[] shape = new long[] {4, 0};
    try (OnnxTensor t = OnnxTensor.createTensor(env, buf, shape)) {
      Assertions.assertArrayEquals(shape, t.getInfo().getShape());
      float[][] output = (float[][]) t.getValue();
      Assertions.assertEquals(4, output.length);
      Assertions.assertEquals(0, output[0].length);
      FloatBuffer fb = t.getFloatBuffer();
      Assertions.assertEquals(0, fb.remaining());
    }
    shape = new long[] {0, 4};
    try (OnnxTensor t = OnnxTensor.createTensor(env, buf, shape)) {
      Assertions.assertArrayEquals(shape, t.getInfo().getShape());
      float[][] output = (float[][]) t.getValue();
      Assertions.assertEquals(0, output.length);
    }
  }

  @Test
  public void testBf16ToFp32() throws OrtException {
    String modelPath = TestHelpers.getResourcePath("/java-bf16-to-fp32.onnx").toString();
    SplittableRandom rng = new SplittableRandom(1);

    float[][] input = new float[10][5];
    ByteBuffer buf = ByteBuffer.allocateDirect(2 * 10 * 5).order(ByteOrder.nativeOrder());
    ShortBuffer shortBuf = buf.asShortBuffer();

    // Generate data
    for (int i = 0; i < input.length; i++) {
      for (int j = 0; j < input[0].length; j++) {
        short bits = (short) rng.nextInt();
        input[i][j] = Fp16Conversions.bf16ToFloat(bits);
        shortBuf.put(bits);
      }
    }
    shortBuf.rewind();

    try (OrtSession.SessionOptions opts = new OrtSession.SessionOptions();
        OrtSession session = env.createSession(modelPath, opts);
        OnnxTensor tensor =
            OnnxTensor.createTensor(env, buf, new long[] {10, 5}, OnnxJavaType.BFLOAT16);
        OrtSession.Result result = session.run(Collections.singletonMap("input", tensor))) {
      OnnxTensor output = (OnnxTensor) result.get(0);
      float[][] outputArr = (float[][]) output.getValue();
      for (int i = 0; i < input.length; i++) {
        Assertions.assertArrayEquals(input[i], outputArr[i]);
      }
    }
  }

  @Test
  public void testFp16ToFp32() throws OrtException {
    String modelPath = TestHelpers.getResourcePath("/java-fp16-to-fp32.onnx").toString();
    SplittableRandom rng = new SplittableRandom(1);

    float[][] input = new float[10][5];
    ByteBuffer buf = ByteBuffer.allocateDirect(2 * 10 * 5).order(ByteOrder.nativeOrder());
    ShortBuffer shortBuf = buf.asShortBuffer();

    // Generate data
    for (int i = 0; i < input.length; i++) {
      for (int j = 0; j < input[0].length; j++) {
        short bits = (short) rng.nextInt();
        input[i][j] = Fp16Conversions.fp16ToFloat(bits);
        shortBuf.put(bits);
      }
    }
    shortBuf.rewind();

    try (OrtSession.SessionOptions opts = new OrtSession.SessionOptions();
        OrtSession session = env.createSession(modelPath, opts);
        OnnxTensor tensor =
            OnnxTensor.createTensor(env, buf, new long[] {10, 5}, OnnxJavaType.FLOAT16);
        OrtSession.Result result = session.run(Collections.singletonMap("input", tensor))) {
      OnnxTensor output = (OnnxTensor) result.get(0);
      float[][] outputArr = (float[][]) output.getValue();
      for (int i = 0; i < input.length; i++) {
        Assertions.assertArrayEquals(input[i], outputArr[i]);
      }
    }
  }

  @Test
  public void testFp32ToFp16() throws OrtException {
    String modelPath = TestHelpers.getResourcePath("/java-fp32-to-fp16.onnx").toString();
    SplittableRandom rng = new SplittableRandom(1);

    int dim1 = 10, dim2 = 5;
    float[][] input = new float[dim1][dim2];
    float[][] expectedOutput = new float[dim1][dim2];
    FloatBuffer floatBuf =
        ByteBuffer.allocateDirect(4 * dim1 * dim2).order(ByteOrder.nativeOrder()).asFloatBuffer();
    ShortBuffer shortBuf = ShortBuffer.allocate(dim1 * dim2);

    // Generate data
    for (int i = 0; i < input.length; i++) {
      for (int j = 0; j < input[0].length; j++) {
        int bits = rng.nextInt();
        input[i][j] = Float.intBitsToFloat(bits);
        floatBuf.put(input[i][j]);
        shortBuf.put(Fp16Conversions.floatToFp16(input[i][j]));
        expectedOutput[i][j] =
            Fp16Conversions.fp16ToFloat(Fp16Conversions.floatToFp16(input[i][j]));
      }
    }
    floatBuf.rewind();
    shortBuf.rewind();

    try (OrtSession.SessionOptions opts = new OrtSession.SessionOptions();
        OrtSession session = env.createSession(modelPath, opts);
        OnnxTensor tensor = OnnxTensor.createTensor(env, floatBuf, new long[] {dim1, dim2});
        OrtSession.Result result = session.run(Collections.singletonMap("input", tensor))) {
      OnnxTensor output = (OnnxTensor) result.get(0);

      // Check outbound Java side cast to fp32 works
      FloatBuffer castOutput = output.getFloatBuffer();
      float[] expectedFloatArr = new float[dim1 * dim2];
      Fp16Conversions.convertFp16BufferToFloatBuffer(shortBuf).get(expectedFloatArr);
      float[] actualFloatArr = new float[dim1 * dim2];
      castOutput.get(actualFloatArr);
      Assertions.assertArrayEquals(expectedFloatArr, actualFloatArr);

      // Check bits are correct
      ShortBuffer outputBuf = output.getShortBuffer();
      short[] expectedShortArr = new short[dim1 * dim2];
      shortBuf.get(expectedShortArr);
      short[] actualShortArr = new short[dim1 * dim2];
      outputBuf.get(actualShortArr);
      Assertions.assertArrayEquals(expectedShortArr, actualShortArr);

      // Check outbound fp16 -> float[] conversion
      float[][] floats = (float[][]) output.getValue();
      for (int i = 0; i < dim1; i++) {
        Assertions.assertArrayEquals(expectedOutput[i], floats[i]);
      }
    }
  }

  @Test
  public void testFp32ToBf16() throws OrtException {
    String modelPath = TestHelpers.getResourcePath("/java-fp32-to-bf16.onnx").toString();
    SplittableRandom rng = new SplittableRandom(1);

    int dim1 = 10, dim2 = 5;
    float[][] input = new float[dim1][dim2];
    float[][] expectedOutput = new float[dim1][dim2];
    FloatBuffer floatBuf =
        ByteBuffer.allocateDirect(4 * dim1 * dim2).order(ByteOrder.nativeOrder()).asFloatBuffer();
    ShortBuffer shortBuf = ShortBuffer.allocate(dim1 * dim2);

    // Generate data
    for (int i = 0; i < input.length; i++) {
      for (int j = 0; j < input[0].length; j++) {
        int bits = rng.nextInt();
        input[i][j] = Float.intBitsToFloat(bits);
        floatBuf.put(input[i][j]);
        shortBuf.put(Fp16Conversions.floatToBf16(input[i][j]));
        expectedOutput[i][j] =
            Fp16Conversions.bf16ToFloat(Fp16Conversions.floatToBf16(input[i][j]));
      }
    }
    floatBuf.rewind();
    shortBuf.rewind();

    try (OrtSession.SessionOptions opts = new OrtSession.SessionOptions();
        OrtSession session = env.createSession(modelPath, opts);
        OnnxTensor tensor = OnnxTensor.createTensor(env, floatBuf, new long[] {dim1, dim2});
        OrtSession.Result result = session.run(Collections.singletonMap("input", tensor))) {
      OnnxTensor output = (OnnxTensor) result.get(0);

      // Check outbound Java side cast to fp32 works
      FloatBuffer castOutput = output.getFloatBuffer();
      float[] expectedFloatArr = new float[dim1 * dim2];
      Fp16Conversions.convertBf16BufferToFloatBuffer(shortBuf).get(expectedFloatArr);
      float[] actualFloatArr = new float[dim1 * dim2];
      castOutput.get(actualFloatArr);
      Assertions.assertArrayEquals(expectedFloatArr, actualFloatArr);

      // Check bits are correct
      ShortBuffer outputBuf = output.getShortBuffer();
      short[] expectedShortArr = new short[dim1 * dim2];
      shortBuf.get(expectedShortArr);
      short[] actualShortArr = new short[dim1 * dim2];
      outputBuf.get(actualShortArr);
      Assertions.assertArrayEquals(expectedShortArr, actualShortArr);

      // Check outbound bf16 -> float[] conversion
      float[][] floats = (float[][]) output.getValue();
      for (int i = 0; i < dim1; i++) {
        Assertions.assertArrayEquals(expectedOutput[i], floats[i]);
      }
    }
  }

  @Test
  public void testFp16RoundTrip() {
    for (int i = 0; i < 0xffff; i++) {
      // Round trip every value
      short curVal = (short) (0xffff & i);
      float upcast = Fp16Conversions.mlasFp16ToFloat(curVal);
      short output = Fp16Conversions.mlasFloatToFp16(upcast);
      if (!Float.isNaN(upcast)) {
        // We coerce NaNs to the same value.
        Assertions.assertEquals(
            curVal,
            output,
            "Expected " + curVal + " received " + output + ", intermediate float was " + upcast);
      }
    }
  }

  @Test
  public void testBf16RoundTrip() {
    for (int i = 0; i < 0xffff; i++) {
      // Round trip every value
      short curVal = (short) (0xffff & i);
      float upcast = Fp16Conversions.bf16ToFloat(curVal);
      short output = Fp16Conversions.floatToBf16(upcast);
      if (!Float.isNaN(upcast)) {
        // We coerce NaNs to the same value.
        Assertions.assertEquals(
            curVal,
            output,
            "Expected " + curVal + " received " + output + ", intermediate float was " + upcast);
      }
    }
  }

  @Test
  public void testClose() throws OrtException {
    long[] input = new long[] {1, 2, 3, 4, 5};
    OnnxTensor value = OnnxTensor.createTensor(env, input);
    assertFalse(value.isClosed());
    long[] output = (long[]) value.getValue();
    assertArrayEquals(input, output);
    value.close();
    // check use after close throws
    assertThrows(IllegalStateException.class, value::getValue);
    // check double close doesn't crash (emits warning)
    TestHelpers.quietLogger(OnnxTensor.class);
    value.close();
    TestHelpers.loudLogger(OnnxTensor.class);
  }
}

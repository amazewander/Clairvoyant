package com.amazewander.clairvoyant;

import java.io.FileWriter;
import java.io.IOException;
import java.io.RandomAccessFile;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

enum SignalStage {
	notStart, findHead, inMiddle
}

public class WaveParser {
	private static final float capacityChangeRate = 0.016f;
	private static final float peakThreshold = 0.2f;

	private static Map<String, String> knownSignals = new HashMap<String, String>();
	private static Map<String, String> knownPrefix = new HashMap<String, String>();
	private static List<String> buttonInitData = new ArrayList<String>();

	private static int lastValueDirection = 0;
	private static int intervalStep = 6;
	private static float baselineValue = 0.0f;
	private static float[] latestValues = new float[10];
	private static int currentArrayHead = 0;
	private static long currentSignalHeadPos;
	private static long currentSignalId = 0;
	private static long fileStartPos;

	private static String getSignalName(String s) {
		for (String key : knownSignals.keySet()) {
			if (s.equals(key) || ((s.length() - key.length() < 5) && s.contains(key))) {
				return knownSignals.get(key);
			}
		}
		return null;
	}

	private static long getDataPos(RandomAccessFile file) throws IOException {
		long pos = 0;
		file.seek(pos);
		int b;
		while ((b = file.read()) != -1) {
			if (b == 97) {
				break;
			}

			pos++;
		}
		return pos + 7;
	}

	private static float readNextFloat(RandomAccessFile file, boolean updateBaseline) {
		int n;
		float f = 0;
		try {
			if ((n = file.readInt()) != -1) {
				f = Float.intBitsToFloat(Integer.reverseBytes(n));
				// System.out.println(f + " " + baselineValue);
			} else {
				System.out.println("file ends!");
				return 0;
			}
		} catch (IOException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}

		if (updateBaseline) {
			updateBaselineValue(f);
		}
		return f;
	}

	private static void updateBaselineValue(float value) {
		baselineValue = (baselineValue * latestValues.length + value - latestValues[currentArrayHead])
				/ latestValues.length;
		latestValues[currentArrayHead] = value;
		currentArrayHead = (currentArrayHead + 1) % latestValues.length;
	}

	/*
	 * private static void updateBaselineValue(float value) { if (baselineValue
	 * > value) { if (lastValueDirection == -1) { float delta = baselineValue -
	 * value; baselineValue -= delta > capacityChangeRate ? capacityChangeRate :
	 * delta; } else { lastValueDirection = -1; } } else if (baselineValue <
	 * value) { if (lastValueDirection == 1) { float delta = value -
	 * baselineValue; baselineValue += delta > capacityChangeRate ?
	 * capacityChangeRate : delta; } else { lastValueDirection = 1; } } else {
	 * lastValueDirection = 0; } }
	 */

	private static boolean skipValue(RandomAccessFile file, int n) throws IOException {
		while (n > 0) {
			readNextFloat(file, true);
			n--;
		}
		return true;
	}

	private static List<Boolean> parseSignalInterval(RandomAccessFile file, float firstValue) throws Exception {
		List<Boolean> signal = null;
		float lastValue = firstValue;
		SignalStage stage = SignalStage.notStart;
		int count = 0;
		int signalStartCount = -1;
		int signalSize = 0;
		int peakOffsetCount = 0;
		int peakOffsetDirection = 0;
		Boolean backupBit = null;
		Boolean isUp = null;
		int finishCount = 0;
		boolean signalValid = true;
		while (true) {
			float v = readNextFloat(file, true);
			if (Math.abs(baselineValue - v) < capacityChangeRate * 3) {
				finishCount++;
				if (finishCount > 5) {
					// System.out.println("----------------------" +
					// getTime(file));
					break;
				}
			} else {
				finishCount = 0;
			}
			switch (stage) {
			case notStart:
				// make sure first signal is 1
				if (lastValue - baselineValue > peakThreshold && v < lastValue) {
					stage = SignalStage.inMiddle;
					signalStartCount = count;
					isUp = false;
					signal = new ArrayList<Boolean>();
					signal.add(true);
					signalSize++;
				} else if (v - baselineValue > peakThreshold) {
					// System.out.println("++++++++++++++++++++++");
					isUp = true;
					stage = SignalStage.findHead;
					signal = new ArrayList<Boolean>();
				}
				break;
			case findHead:
				// System.out.println("///////////////////////");
				if (((v < lastValue) && isUp) || ((v > lastValue) && !isUp)) {
					stage = SignalStage.inMiddle;
					signalStartCount = count;
				}
			case inMiddle:
				if ((count - signalStartCount + 1) / 6 > signalSize) {
					if (backupBit != null) {
						signal.add(backupBit);
						backupBit = null;
						signalSize++;
						// System.out.println("<<<<<<<<<<<<<<<<"+backupBit);
					} else {
						// System.err.println("lost one bit!!!");
					}
				}
				if ((count - signalStartCount + 1) % 6 <= 2 && (count - signalStartCount + 1) / 6 >= signalSize
						&& (((v < lastValue) && isUp) || ((v > lastValue) && !isUp))) {
					if (Math.abs(lastValue - baselineValue) > peakThreshold) {
						int offset = (count - signalStartCount + 1) % 6;
						// if (peakOffsetDirection == 1 && offset == 0) {
						// break;
						// }
						if (offset - 1 == peakOffsetDirection) {
							peakOffsetCount++;
							if (peakOffsetDirection != 0 && peakOffsetCount > 0) {
								signalStartCount += peakOffsetDirection;
								peakOffsetCount = 0;
							}
						} else {
							peakOffsetCount = 0;
							peakOffsetDirection = offset - 1;
						}
						if (lastValue > baselineValue) {
							// System.out.println("=======" + count +
							// "==========" + signalStartCount + "=======");
							signal.add(true);
							signalSize++;
						} else if (lastValue < baselineValue) {
							// System.out.println(">>>>>>>>" + count +
							// ">>>>>>>>>" + signalStartCount + ">>>>>>>>");
							signal.add(false);
							signalSize++;
						}
					} else if (Math.abs(baselineValue - lastValue) < capacityChangeRate) {
						System.out.println("signal over, " + lastValue + ", time : " + getTime(file));
						signalValid = false;
					} else {
						if ((count - signalStartCount + 1) % 6 >= 1) {
							if (lastValue < baselineValue) {
								// System.out
								// .println("********" + count + "*************"
								// + signalStartCount + ">>>>>>>>");
								signal.add(false);
								signalSize++;
							} else {
								// System.out.println("********" + count +
								// "==========" + signalStartCount + "=======");
								signal.add(true);
								signalSize++;
							}
						} else {
							backupBit = lastValue > baselineValue;
						}
						// try {
						// System.out.println("value invalid:" + baselineValue +
						// ", " + lastValue + "," + file.getFilePointer()
						// + ", " + currentSignalId + ", " + count + ", " +
						// getTime(file));
						// // debugValue(file, 14, 3);
						// } catch (IOException e) {
						// e.printStackTrace();
						// }
					}
				}
				break;
			default:
				break;

			}
			if (!signalValid) {
				break;
			}
			if (v != lastValue) {
				isUp = v > lastValue;
			}
			lastValue = v;
			count++;
			// System.out.println("count : " + count);
		}
		return signal;
	}

	private static long getTime(RandomAccessFile file) throws IOException {
		return (file.getFilePointer() - fileStartPos) / 4 / 6;
	}

	private static List<Boolean> getNextSignal(RandomAccessFile file) throws Exception {
		float v;
		// System.out.println("!!!" + v);
		// boolean isUp = true;
		List<Boolean> signal = null;
		while (signal == null) {
			do {
				v = readNextFloat(file, true);
			} while (v - baselineValue < capacityChangeRate);
			signal = parseSignalInterval(file, v);
		}
		String s = getSignalString(signal);
		/*
		 * if (!s.equals("11111100001111111100111111111111111100000000000011"))
		 * { System.out.println(s); }
		 */
		/*
		 * if(s.length()>50){ System.out.println(s); }
		 */
		return signal;
	}

	private static String getSignalString(List<Boolean> signal) {
		if (signal == null || signal.size() <= 0) {
			return null;
		}
		StringBuffer sb = new StringBuffer();
		for (Boolean s : signal) {
			sb.append(s ? 1 : 0);
		}
		return sb.toString();
	}

	private static void signalStreamParse(RandomAccessFile file, int startTime, int signalLimit, int endTime,
			int lengthLimit) throws IOException {
		System.out.println(startTime + " " + endTime);
		// fileStartPos;
		file.seek(fileStartPos + startTime * 6 * 4);
		long endPos = fileStartPos + endTime * 6 * 4;

		skipValue(file, 15); // initialize baseline value

		FileWriter fileWritter = new FileWriter("output.txt", false);
		Map<String, Integer[]> signalMap = new HashMap<String, Integer[]>();
		List<Boolean> signal;
		int idCount = 0;
		try {
			while ((signalLimit == 0 || idCount < signalLimit) && (endTime == 0 || file.getFilePointer() < endPos)) {
				signal = getNextSignal(file);
				String signalStr = getSignalString(signal);
				if (lengthLimit > 0 && signalStr.length() < lengthLimit) {
					continue;
				}

				String name = getSignalName(signalStr);
				if (name != null) {
					if (!name.equals("Common heart beat")) {
						System.out.println(name);
						// printSignal(signalStr);
					}
				} else {
					// System.out.println("unknown : " + signalStr);
					printSignal(signalStr);
				}

				if (signalMap.containsKey(signalStr)) {
					Integer[] info = signalMap.get(signalStr);
					info[1] = info[1] + 1;
					fileWritter.write(info[0] + ",");
				} else {
					// System.out.println("new signal : " + signalStr);
					signalMap.put(signalStr, new Integer[] { idCount, 1 });
				}
				idCount++;
			}
		} catch (Exception e1) {
			// TODO Auto-generated catch block
			e1.printStackTrace();
			try {
				fileWritter.close();
			} catch (IOException e) {
				// TODO Auto-generated catch block
				e.printStackTrace();
			}
			return;
		}

		for (String key : signalMap.keySet()) {
			// System.out.println(key + " : " + signalMap.get(key)[0] + " : " +
			// signalMap.get(key)[1]);
		}

		try {
			fileWritter.close();
		} catch (IOException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
		System.out.println(
				"***********************************************************************************************************************************************");
	}

	private static void printSignal(String signal) {
		if (signal.length() > 120) {
			String sub = signal.substring(0, 50);
			System.out.print(sub);
			sub = signal.substring(50, 54);
			System.out.print("[" + sub + "]");
			sub = signal.substring(54, 56);
			System.out.print(sub);
			sub = signal.substring(56, 66);
			System.out.print("[" + sub + "]");
			sub = signal.substring(66, 106);
			System.out.print(sub);
			sub = signal.substring(106, 114);
			System.out.print("[" + sub + "]");
			sub = signal.substring(114);
			System.out.print(sub);

			for (String key : knownPrefix.keySet()) {
				if (signal.startsWith(key)) {
					System.out.print("  " + knownPrefix.get(key));
				}
			}

			String data = signal.substring(66);
			if (data.startsWith("00000000110011110000000011")) {
				int level = 0;
				for (int i = 106; i < 114; i += 2) {
					level *= 2;
					if (signal.charAt(i) == '1') {
						level += 1;
					}
				}
				System.out.print("  [level " + level + "]");
			} else if (data.startsWith("00001100000011110000000011")) {
				System.out.print("  [level to limit]");
			} else if (signal.startsWith("111111000011110000001100000000000011000011110011001111")
					&& data.startsWith("0000000000000011001100110000000000000000000000")) {
				System.out.print("  [shift response A with " + signal.charAt(112) + "]");
			} else if (signal.startsWith("111111000011111100000000001111001100000000000000111111")
					&& data.startsWith("0000000000000011001100110000110000000000000000")) {
				System.out.print("  [shift response A_g with " + signal.charAt(112) + "]");
			} else if (signal.startsWith("111111000011111100000011111111111111000000000000111111")
					&& data.startsWith("000000000000110000000000000000000011000000000000")) {
				System.out.print("  [finish g2]");
			} else if (signal.startsWith("111111000011110000001100000000000011000011110011001111")
					&& data.startsWith("000000000000001100000000000011001111110000111100")) {
				System.out.print("  [finish g2_2]");
			} else if (signal.startsWith("111111000011111100000011111111111111000000000000111100")
					&& data.startsWith("0011000011001100000000000000000000000000")) {
				System.out.print("  [finish g2_A]");
			} else if (signal.startsWith("111111000011111100000011111111111111000000000000110000")
					&& data.startsWith("0000000000000011000000000000001100000000")) {
				System.out.print("  [finish g2_B]");
			} else if (signal.startsWith("111111000011111100000011111111111111000000000000110011")
					&& data.startsWith("0000000000000000000000110000001100000000")) {
				System.out.print("  [finish g2_C]");
			} else if (signal.startsWith("111111000011111100000011111111111111000000000000110011")
					&& data.startsWith("0000000000000000000000110011110000000000")) {
				System.out.print("  [finish g2_unfinished_C]");
			} else {
				for (int i = 0; i < buttonInitData.size(); i++) {
					if (data.startsWith(buttonInitData.get(i))) {
						System.out.print("  [button init signal " + i + "]");
					}
				}
			}

			System.out.println();
		} else {
			System.out.println(signal);
		}
	}

	public static void main(String[] args) throws Exception {
		knownPrefix.put("11111100001111000000110011000011110000001111001100111100", "before c/after prefix");
		knownPrefix.put("111111000011110000001100001111001100001100001111000011", "before b prefix");
		knownPrefix.put("111111000011110000001100001111001100001100001111001100", "before a prefix");
		knownPrefix.put("111111000011110000001100000000000011001100001111001111", "Group H - far a prefix");
		knownPrefix.put("111111000011111100000000110000111100000000000000111111", "Group G - far b prefix");
		knownPrefix.put("111111000011110000001100000000000011000011110011001111", "- 110");

		knownPrefix.put("11111100001111110000001111111111111100000000000011", "g2");
		knownPrefix.put("11111100001111000000110000000000001100111111111100110011", "c");
		knownPrefix.put("11111100001111110000000000001111110000000000000011110011", "d");
		knownPrefix.put("11111100001111000000110000000000001100001111001100110000", "- 100");
		knownPrefix.put("11111100001111000000110000000000001100001111001100", "shift button prefix");
		knownPrefix.put("11111100001111110000000000111100110000000000000011", "g");
		knownPrefix.put("11111100001111110000000011110011111100000000000011", "g1");
		knownPrefix.put("11111100001111000000110000000000001100111100111111", "h");
		knownPrefix.put("11111100001111000000110000000000001100110000111100", "h - button init");
		knownPrefix.put("11111100001111110000000011111111110000000000000011001111", "some multiply broadcast prefix");

		knownSignals.put("11111100001111111100111111111111111100000000000011", "Common heart beat");
		knownSignals.put("11111100001111000011000011000011110000111100111111", "Response A");
		knownSignals.put("11111100001111000011000011000011110000001111001100", "New response A");
		knownSignals.put("11111100001111000011000011110011111100110000111100", "Response B");
		knownSignals.put("11111100001111000011000000111100110000110000111100", "New response B");
		knownSignals.put("11111100001111000011000011000011110000000000000011", "Far before down response A");
		knownSignals.put("11111100001111000011000000000000001100110000111100", "Far before down response B");
		knownSignals.put("11111100001111000011000011110011111100000000000011", "EW90 response A");
		knownSignals.put("11111100001111000011000000000000001100110000111100", "EW90 response B");
		knownSignals.put("11111100001111000011000000000000001100001111001100", "EW90 response C");
		knownSignals.put("11111100001111000011000000111100110000000000000011", "Shift button response");
		knownSignals.put("11111100001111000011000000001111110000000000000011", "in/out response A");
		knownSignals.put("11111100001111000011000000000000001100000011111100", "in/out response B");
		knownSignals.put("11111100001111000011000011111111110000000000000011", "init response A");
		knownSignals.put("11111100001111000011000000000000001100111111111100", "init response B");

		buttonInitData.add("000000000000001100000011110000000000000000110011");
		buttonInitData.add("000000000000001100111100111111000011001100111100");
		buttonInitData.add("000000000000001100001100111111000000111100000011");
		buttonInitData.add("000000000000000000000000000000001111110011000011");
		buttonInitData.add("000000000000001100000000111111000000000011111111");
		buttonInitData.add("000000000000000000000000000000000000000000000000");
		buttonInitData.add("000000000000001100110011111111000000000000000000");
		buttonInitData.add("000000000000001100001100001111000000000000000000");
		buttonInitData.add("000000000000001100000011111100000000000000000000");

		// knownSignals.put(
		// "1111110000111100000011001111001111110011000011110011000011110000110000000011001111000000001100000000000000000011111100001100110000",
		// "Before down 1");
		// knownSignals.put(
		// "1111110000111100000011001111001111110011000011110000110011110011000000000011001111000000000000000000000000000000001100110000110011",
		// "Before down 2");
		// knownSignals.put(
		// "1111110000111100000011001100001111000011110011111111110011110000110000000011001111000000001100110000000000000011001100110011001100",
		// "Before down 3");
		//
		// knownSignals.put(
		// "1111110000111100000011001111001111110011000011110011000011111100000000000011001111000000001100000000000000001100001111000011111111",
		// "Before down 1 - 2");
		// knownSignals.put(
		// "1111110000111100000011001111001111110011000011110000110011111100110000000011001111000000000000000000000000000000001111000000111111",
		// "Before down 2 - 2");
		// knownSignals.put(
		// "1111110000111100000011001100001111000011110011111111110011110011110000000011001111000000001100110000000000000011111100000000000011",
		// "Before down 3 - 2");
		//
		// knownSignals.put(
		// "1111110000111100000011001111001111110011000011110011000011111111110000000011001111000000001100000000000000001100111111111100001100",
		// "Before down 1 - 3");
		// knownSignals.put(
		// "1111110000111100000011001111001111110011000011110000110000000000000000000011001111000000000000000000000000000000000011001111111100",
		// "Before down 2 - 3");
		// knownSignals.put(
		// "1111110000111100000011001100001111000011110011111111110011111100110000000011001111000000001100110000000000001100001111110000000000",
		// "Before down 3 - 3");
		//
		// knownSignals.put(
		// "1111110000111100000011000000000000110011000011110011110011110000000000110000111100001100000011000000000000000000001111000011111111",
		// "Far before down 1");
		// knownSignals.put(
		// "1111110000111111000000001100001111000000000000001111110000110011000000110000111100001100000011110000110011110011001111001111111111",
		// "Far before down 2");
		//
		// knownSignals.put(
		// "1111110000111100000011000000000000110011000011110011110011110011110000110000111100001100000011000000000000000000001111111100110011",
		// "Far before down 1 - 2");
		// knownSignals.put(
		// "1111110000111111000000001100001111000000000000001111110000110011110000110000111100001100000011110000110011110011001111000011000011",
		// "Far before down 2 - 2");
		//
		// knownSignals.put(
		// "1111110000111100000011000000000000110011000011110011110011111111000000110000111100001100000011000000000000000000001100111111001111",
		// "Far before down 1 - 3");
		// knownSignals.put(
		// "1111110000111111000000001100001111000000000000001111110000111100000000110000111100001100000011110000110011110011001100110011001111",
		// "Far before down 2 - 3");
		//
		// knownSignals.put(
		// "1111110000111100000011001100001111000011110011111111110011110011000000000011001111000000001100110000000000000011111100001100111111",
		// "After down 1");
		//
		// knownSignals.put(
		// "1111110000111100000011001100001111000011110011111111110011111100000000000011001111000000001100110000000000001100001111111100111100",
		// "After down 1 - 2");
		//
		// knownSignals.put(
		// "1111110000111100000011001100001111000011110011111111110011111111000000000011001111000000001100110000000000001100111111001111110011",
		// "After down 1 - 3");
		//
		// knownSignals.put(
		// "1111110000111100000011000000000000110000111100110011110011001111110000000000110000000000000000000000000000001100000011001111001111",
		// "shift button A");

		RandomAccessFile file = new RandomAccessFile(
				"F:/constructions/bike_parts/Clairvoyant/up_down_ew90.wav", "r");
		fileStartPos = getDataPos(file);
		// signalStreamParse(file, 597070, 1);
		int limit = 0;
		// signalStreamParse(file, 1075920, 5, 0); // far before down 1
		// signalStreamParse(file, 1095700, 28, 0); // before down 1
		// signalStreamParse(file, 1099200, 15, 0); // after down 1
		// signalStreamParse(file, 1172120, 5, 0); // after far down 1
		// signalStreamParse(file, 16911800, 200, 0); //

		// signalStreamParse(file, 2582540, 1, 0);
		// signalStreamParse(file, 10902450, 1, 0);
		// signalStreamParse(file, 14142700, 1, 0);
		// signalStreamParse(file, 17322500, 1, 0);
		// signalStreamParse(file, 20539000, 1, 0);
		// signalStreamParse(file, 23742600, 1, 0);
		// signalStreamParse(file, 26917000, 1, 0);
		// signalStreamParse(file, 30096900, 1, 0);
		// signalStreamParse(file, 33271300, 1, 0);
		// signalStreamParse(file, 36471600, 1, 0);
		// signalStreamParse(file, 39663700, 1, 0);

		// whole_up.wav
		// signalStreamParse(file, 2582540+80000, 1, 0, 120);
		// signalStreamParse(file, 10902450+80000, 1, 0, 120);
		// signalStreamParse(file, 14142700+80000, 1, 0, 120);
		// signalStreamParse(file, 17322500+80000, 1, 0, 120);
		// signalStreamParse(file, 20539000+80000, 1, 0, 120);
		// signalStreamParse(file, 23742600+80000, 1, 0, 120);
		// signalStreamParse(file, 26917000+80000, 1, 0, 120);
		// signalStreamParse(file, 30096900+80000, 1, 0, 120);
		// signalStreamParse(file, 33271300+80000, 1, 0, 120);
		// signalStreamParse(file, 36471600+80000, 1, 0, 120);
		// signalStreamParse(file, 39663700+80000, 1, 0, 120);

		// signalStreamParse(file, 47897300, 5, 0, 120);
		// signalStreamParse(file, 51106300, 5, 0, 120);
		// signalStreamParse(file, 54301250, 5, 0, 120);

		// whole_down.wav
		// signalStreamParse(file, 2938290+80000, 1, 0, 120);
		// signalStreamParse(file, 6176430+80000, 1, 0, 120);
		// signalStreamParse(file, 9386480+80000, 1, 0, 120);
		// signalStreamParse(file, 12599780+80000, 1, 0, 120);
		// signalStreamParse(file, 15781120+80000, 1, 0, 120);
		// signalStreamParse(file, 18979490+80000, 1, 0, 120);
		// signalStreamParse(file, 22153910+80000, 1, 0, 120);
		// signalStreamParse(file, 25335880+80000, 1, 0, 120);
		// signalStreamParse(file, 28514610+80000, 1, 0, 120);

		// up_down_ew90.wav
		 signalStreamParse(file, 795140, 40, 0, 0);
		 /*signalStreamParse(file, 2404500, 6, 0, 0);
		 signalStreamParse(file, 4008800, 6, 0, 0);
		 signalStreamParse(file, 5743200, 6, 0, 0);
		 signalStreamParse(file, 7428500, 6, 0, 0);
		 signalStreamParse(file, 8840900, 6, 0, 0);
		 signalStreamParse(file, 10476500, 6, 0, 0);
		 signalStreamParse(file, 11916500, 6, 0, 0);
		 signalStreamParse(file, 13600500, 6, 0, 0);
		 signalStreamParse(file, 15204600, 6, 0, 0);
		
		
		 signalStreamParse(file, 19309400, 6, 0, 0);
		 signalStreamParse(file, 20848700, 6, 0, 0);
		 signalStreamParse(file, 22510700, 6, 0, 0);
		 signalStreamParse(file, 24111800, 6, 0, 0);
		 signalStreamParse(file, 25776000, 6, 0, 0);
		 signalStreamParse(file, 27494400, 6, 0, 0);
		 signalStreamParse(file, 29140400, 6, 0, 0);
		 signalStreamParse(file, 30742900, 6, 0, 0);
		 signalStreamParse(file, 32408400, 6, 0, 0);
		 signalStreamParse(file, 33955800, 6, 0, 0);

		 signalStreamParse(file, 33955800, 20, 0, 0);*/

		// up_down_ew90_v2.wav
		// signalStreamParse(file, 1351000, 20, 0, 0);
		// signalStreamParse(file, 2316800, 20, 0, 0);
		// signalStreamParse(file, 4008800, 2, 0, 120);
		// signalStreamParse(file, 5743200, 2, 0, 120);
		// signalStreamParse(file, 7428500, 2, 0, 120);
		// signalStreamParse(file, 8840900, 2, 0, 120);
		// signalStreamParse(file, 10476500, 2, 0, 120);
		// signalStreamParse(file, 11916500, 2, 0, 120);
		// signalStreamParse(file, 13600500, 2, 0, 120);
		// signalStreamParse(file, 15204600, 2, 0, 120);

		// ew90_in_out.wav
		// signalStreamParse(file, 4744600, 3, 0, 0);
		// signalStreamParse(file, 4922100, 6, 0, 0);
		// signalStreamParse(file, 4933350, 1, 0, 0);
		// signalStreamParse(file, 4944300, 1, 0, 0);
		// signalStreamParse(file, 4953300, 7, 0, 0);
		// signalStreamParse(file, 4985400, 22, 0, 0);
		// signalStreamParse(file, 5020300, 1, 0, 0);
		// signalStreamParse(file, 5046360, 61, 0, 0);

		// signalStreamParse(file, 6051500, 9, 0, 0);
		// signalStreamParse(file, 10333000, 20, 0, 0);
		//
		//
		// signalStreamParse(file, 10422500, 18, 0, 0);
		// signalStreamParse(file, 11035400, 9, 0, 0);
		// signalStreamParse(file, 11867500, 9, 0, 0);

		// signalStreamParse(file, 14980000, 8, 0, 0);
		// signalStreamParse(file, 16041200, 16, 0, 0);
		// signalStreamParse(file, 25230800, 1, 0, 0);

		// ew90_in_out2.wav
		// signalStreamParse(file, 4371800, 3, 0, 0);
		// signalStreamParse(file, 4647600, 6, 0, 0);
		// signalStreamParse(file, 4658800, 1, 0, 0);
		// signalStreamParse(file, 4668600, 1, 0, 0);
		// signalStreamParse(file, 4678800, 7, 0, 0);
		// signalStreamParse(file, 4710200, 25, 0, 0);
		// signalStreamParse(file, 4745700, 1, 0, 0);
		// signalStreamParse(file, 4771800, 61, 0, 0);
		// signalStreamParse(file, 5188500, 13, 0, 0);
		// signalStreamParse(file, 5778100, 9, 0, 0);
		// signalStreamParse(file, 8449800, 20, 0, 0);
		// signalStreamParse(file, 12375600, 20, 0, 0);
		// signalStreamParse(file, 13419700, 5, 0, 0);
		// signalStreamParse(file, 14551400, 5, 0, 0);

		// signalStreamParse(file, 3541300, 5, 0, 0);
		// signalStreamParse(file, 8412700, 5, 0, 0);
		// signalStreamParse(file, 13502800, 5, 0, 0);
		// signalStreamParse(file, 33179800, 5, 0, 0);

		// ew90_in_out3.wav
		// signalStreamParse(file, 17269100, 18, 0, 0);
		// signalStreamParse(file, 20383900, 18, 0, 0);
		// signalStreamParse(file, 36323500, 27, 0, 0);
		// signalStreamParse(file, 37468250, 20, 0, 0);

		// test_run.wav
		// signalStreamParse(file, 18215400, 5, 0, 0);

		// button_in.wav
		// signalStreamParse(file, 11051600, 3, 0, 0);
		// signalStreamParse(file, 11237800, 5, 0, 0);
		// signalStreamParse(file, 11250100, 1, 0, 0);
		// signalStreamParse(file, 11261000, 1, 0, 0);
		// signalStreamParse(file, 11270100, 7, 0, 0);
		// signalStreamParse(file, 11302300, 52, 0, 0);
		// signalStreamParse(file, 11348400, 1, 0, 0);
		// signalStreamParse(file, 11374450, 1, 0, 0);
		// signalStreamParse(file, 12380100, 6, 0, 0);

		// button_ew90_in_whole_up_down.wav
		// button init
		// signalStreamParse(file, 5611900, 3, 0, 0);
		// signalStreamParse(file, 5799000, 6, 0, 0);
		// signalStreamParse(file, 5830400, 8, 0, 0);
		// signalStreamParse(file, 5862600, 60, 0, 0);
		// signalStreamParse(file, 6939300, 7, 0, 0);
		// System.out.println("----------------------------------------------");
		// // ew90 init
		// signalStreamParse(file, 14754800, 3, 0, 0);
		// signalStreamParse(file, 14934600, 5, 0, 0);
		// signalStreamParse(file, 14966800, 8, 0, 0);
		// signalStreamParse(file, 14999300, 30, 0, 0);
		// signalStreamParse(file, 15060900, 55, 0, 0);
		// signalStreamParse(file, 15571600, 14, 0, 0);
		// signalStreamParse(file, 16067200, 9, 0, 0);
		// System.out.println("----------------------------------------------");
		// // level down
		// signalStreamParse(file, 19973100, 3, 0, 120);
		// signalStreamParse(file, 20992500, 3, 0, 120);
		// signalStreamParse(file, 21952600, 3, 0, 120);
		// signalStreamParse(file, 22914100, 3, 0, 120);
		// signalStreamParse(file, 23883700, 3, 0, 120);
		// signalStreamParse(file, 24817100, 3, 0, 120);
		// signalStreamParse(file, 25823800, 3, 0, 120);
		// signalStreamParse(file, 26804400, 3, 0, 120);
		// signalStreamParse(file, 27693400, 3, 0, 120);
		// signalStreamParse(file, 28883000, 3, 0, 120);
		// System.out.println("----------------------------------------------");
		// signalStreamParse(file, 20072000, 10, 0, 0);
		// signalStreamParse(file, 20173400, 3, 0, 0);
		// signalStreamParse(file, 20273000, 3, 0, 0);
		// signalStreamParse(file, 20343000, 5, 0, 0);
		// signalStreamParse(file, 20675800, 10, 0, 0);
		// // level up
		// signalStreamParse(file, 31108300, 1, 0, 0);
		// signalStreamParse(file, 31977300, 1, 0, 0);
		// signalStreamParse(file, 33026600, 1, 0, 0);
		// signalStreamParse(file, 33996600, 1, 0, 0);
		// signalStreamParse(file, 34989100, 1, 0, 0);
		// signalStreamParse(file, 36029900, 1, 0, 0);
		// signalStreamParse(file, 37165500, 1, 0, 0);
		// signalStreamParse(file, 38132200, 1, 0, 0);
		// signalStreamParse(file, 39181200, 1, 0, 0);
		// signalStreamParse(file, 40260200, 3, 0, 120);
		// System.out.println("----------------------------------------------");

		// without_ew90_signal_analyse.wav
		// signalStreamParse(file, 17738900, 20, 0, 0);
		// signalStreamParse(file, 17821000, 2, 0, 0);
		// signalStreamParse(file, 17839600, 7, 0, 0);

		// signalStreamParse(file, 17939750, 2, 0, 0);
		// signalStreamParse(file, 18031700, 2, 0, 0);
		// signalStreamParse(file, 18095400, 4, 0, 0);
		// signalStreamParse(file, 18443000, 10, 0, 0);
		// System.out.println("----------------------------------------------");
		// signalStreamParse(file, 25943400, 20, 0, 0);
		// signalStreamParse(file, 26023400, 2, 0, 0);
		// signalStreamParse(file, 26043200, 13, 0, 0);

		// signalStreamParse(file, 26143500, 7, 0, 0);
		// signalStreamParse(file, 26190700, 2, 0, 0);
		// signalStreamParse(file, 26242500, 4, 0, 0);
		// signalStreamParse(file, 26647400, 0, 26718400, 0);
		// signalStreamParse(file, 26728200, 0, 26744400, 0);
		// signalStreamParse(file, 26757700, 0, 26788500, 0);
		// signalStreamParse(file, 27032000, 0, 27043000, 0);
		// signalStreamParse(file, 27837000, 7, 0, 0);
		// signalStreamParse(file, 27861000, 7, 0, 0);
		// signalStreamParse(file, 27885800, 0, 27961400, 0);

		// only_button_success_run.wav
		// signalStreamParse(file, 7727660, 30, 0, 0);
		// signalStreamParse(file, 7789700, 5, 0, 0);
		// signalStreamParse(file, 7843200, 7, 0, 0);
		// System.out.println("----------------------------------------------");

		// only_button_no_derailleur_in.wav
		// signalStreamParse(file, 4184200, 5, 0, 0);
		// signalStreamParse(file, 4216550, 8, 0, 0);
		// signalStreamParse(file, 4247600, 0, 4272700, 0);

		// power_on_with_button.wav
		// signalStreamParse(file, 4100100, 5, 0, 0);
		// signalStreamParse(file, 4132300, 8, 0, 0);
		// signalStreamParse(file, 4164400, 0, 4189400, 0);
		// System.out.println("----------------------------------------------");

		// signalStreamParse(file, 4208540, 2, 0, 120);
		//
		// signalStreamParse(file, 4298400, 5, 0, 0);
		// signalStreamParse(file, 4330700, 8, 0, 0);
		// signalStreamParse(file, 4362900, 0, 4386900, 0);
		// signalStreamParse(file, 4431000, 0, 4446800, 0);

		// power_on_without_button.wav
		// signalStreamParse(file, 3959940, 5, 0, 0);
		// signalStreamParse(file, 3992200, 8, 0, 0);
		// signalStreamParse(file, 4024300, 0, 4047300, 0);
		// signalStreamParse(file, 4093700, 0, 4107800, 0);

		// finish_signal_analyse1.wav
		// signalStreamParse(file, 19316600, 0, 19319950, 120);
		// signalStreamParse(file, 19370500, 1, 0, 120);
		// signalStreamParse(file, 19475000, 2, 0, 120);
		// signalStreamParse(file, 19696600, 2, 0, 120);
		// signalStreamParse(file, 20077250, 1, 0, 120);
		// signalStreamParse(file, 20266500, 2, 0, 120);
		// System.out.println("----------------------------------------------");
		// signalStreamParse(file, 23118200, 5, 0, 120);
		// signalStreamParse(file, 23159100, 1, 0, 120);
		// signalStreamParse(file, 23483400, 2, 0, 120);
		// signalStreamParse(file, 23858400, 1, 0, 120);
		// System.out.println("----------------------------------------------");
		// signalStreamParse(file, 9074500, 3, 0, 120);
		// signalStreamParse(file, 9131200, 1, 0, 120);
		// signalStreamParse(file, 9236000, 2, 0, 120);
		// signalStreamParse(file, 9854500, 1, 0, 120);

		// pce_debug_finish.wav
		// signalStreamParse(file, 9571300, 10, 0, 0);
		// signalStreamParse(file, 9590360, 17, 0, 0);
		// signalStreamParse(file, 9608750, 0, 9636100, 0);
		// signalStreamParse(file, 10617950, 0, 10741500, 120);

		// finish_signal_analyse2.wav
		// signalStreamParse(file, 2662000, 1, 0, 0);
		// signalStreamParse(file, 2749000, 10, 0, 0);
		// signalStreamParse(file, 2848000, 5, 0, 0);

		// signalStreamParse(file, 5084200, 7, 0, 0);
		// signalStreamParse(file, 5114000, 4, 0, 0);
		// signalStreamParse(file, 5744800, 4, 0, 0);

		// signalStreamParse(file, 8128600, 7, 0, 0);
		// signalStreamParse(file, 8157500, 4, 0, 0);
		// signalStreamParse(file, 8789000, 4, 0, 0);

		// signalStreamParse(file, 10536800, 7, 0, 0);
		// signalStreamParse(file, 10579000, 4, 0, 0);
		// signalStreamParse(file, 10586400, 2, 0, 0);
		// signalStreamParse(file, 10743200, 4, 0, 0);

		// signalStreamParse(file, 14039000, 5, 0, 0);
		// signalStreamParse(file, 14045800, 2, 0, 0);
		// signalStreamParse(file, 10743200, 4, 0, 0);

		// signalStreamParse(file, 17195200, 7, 0, 0);
		// signalStreamParse(file, 17206400, 4, 0, 0);
		// signalStreamParse(file, 17253400, 4, 0, 0);
		
//		signalStreamParse(file, 21319500, 7, 0, 0);
//		signalStreamParse(file, 21374000, 2, 0, 0);
//		signalStreamParse(file, 21477900, 4, 0, 0);
//		System.out.println("\n\n----------------------------------------------\n\n");
//		signalStreamParse(file, 38612500, 7, 0, 0);
//		signalStreamParse(file, 38673800, 2, 0, 0);
//		signalStreamParse(file, 38779000, 4, 0, 0);
//		signalStreamParse(file, 39002800, 4, 0, 0);
//		signalStreamParse(file, 39553000, 4, 0, 0);
		
		//microstep_minus5.wav
//		signalStreamParse(file, 2080550, 5, 0, 120);
//		signalStreamParse(file, 3520200, 5, 0, 120);
//		signalStreamParse(file, 4840600, 5, 0, 120);
//		signalStreamParse(file, 6033400, 5, 0, 120);
//		signalStreamParse(file, 7235500, 5, 0, 120);
//		signalStreamParse(file, 8525000, 5, 0, 120);
//		signalStreamParse(file, 10022600, 5, 0, 120);
//		signalStreamParse(file, 11125000, 5, 0, 120);
//		signalStreamParse(file, 12359500, 5, 0, 120);
//		signalStreamParse(file, 13546400, 5, 0, 120);
//		signalStreamParse(file, 26781000, 5, 0, 120);
		
		//test2.wav
//		signalStreamParse(file, 1890000, 5, 0, 120);
//		signalStreamParse(file, 4313100, 5, 0, 120);
//		signalStreamParse(file, 5834300, 5, 0, 120);
//		signalStreamParse(file, 8545200, 5, 0, 120);
		
		//adjust.wav
//		signalStreamParse(file, 8619000, 5, 0, 120);
//		signalStreamParse(file, 10130100, 5, 0, 120);
//		signalStreamParse(file, 11549300, 5, 0, 120);
//		signalStreamParse(file, 14409400, 5, 0, 120);
		
		
		// checkSum(
		// "1111110000111100000011000000000000110011000011110011110000000000110000110000111100001100000011000000000000000000000000000011111100",
		// 0, 114, 114);

		// for (int i = 0; i < 82; i += 2) {
		//
		// int startIndex = i;
		//
		// for (int j = i + 64; j < 114; j += 2) {
		// int endIndex = j;
		//
		// for (int k = 104; k <= 114; k += 2) {
		// if (checkSum(
		// "1111110000111100000011000000000000110011000011110011110000000000110000110000111100001100000011000000000000000000000000000011111100",
		// startIndex, endIndex, k)
		// && checkSum(
		// "1111110000111100000011000000000000110011000011110011110000001100000000110000111100001100000011000000000000000000000011000000000000",
		// startIndex, endIndex, k)
		// && checkSum(
		// "1111110000111100000011000000000000110011000011110011110000001111110000110000111100001100000011000000000000000000000011111111001100",
		// startIndex, endIndex, k)) {
		// System.out.println(startIndex + " " + endIndex + " " + k);
		// }
		// }
		// }
		// System.out.println("----------------------");
		// }

		// signalGenerationCommands(
		// "1111110000111111000000001100001111000000000000001111110011110011000000000011001111000000001100000000000000110011110011001100111111");
		/*
		 * System.out.println("--------------------------------");
		 * signalStreamParse(file, 2660950, limit);
		 * System.out.println("--------------------------------");
		 * signalStreamParse(file, 4536700, limit);
		 * System.out.println("--------------------------------");
		 * signalStreamParse(file, 6400320, limit);
		 */

		// file.seek(4472018);
		// System.out.println(getNextSignal(file));
	}
}

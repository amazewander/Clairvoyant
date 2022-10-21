package com.amazewander.clairvoyant;

import java.io.BufferedReader;
import java.io.FileReader;
import java.io.IOException;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

public class CheckBitAnalyser {

	private static boolean checkCRC(String signal, int startIndex, int endIndex, int crcStartIndex, int hx) {
		int temp = 0;

		if (signal.length() != 65) {
			System.err.println("length not right!" + signal.length());
		} else {
			int i = startIndex;
			while (true) {
				boolean ready = false;
				while (!ready) {
					if (temp >= 128) {
						ready = true;
					}

					char c = '0';
					if (i < endIndex) {
						c = signal.charAt(i);
					} else if (i >= endIndex + 8) {
						break;
					}

					temp = temp << 1;
					temp &= ~256;
					if (c == '1') {
						temp = temp | 1;
					}
					// System.out.println(Integer.toBinaryString(temp) + " " +
					// c);
					i++;
				}
				if (i >= endIndex + 8) {
					break;
				}
				temp = temp ^ hx;
				// System.out.println("00" + Integer.toBinaryString(hx));
				// System.out.println(Integer.toBinaryString(temp));
			}

			// System.out.println(Integer.toBinaryString(temp));

			int crc = 0;
			for (int j = crcStartIndex; j < crcStartIndex + 8; j++) {
				crc = crc << 1;
				if (signal.charAt(j) == '1') {
					crc = crc | 1;
				}
			}

			// System.out.println("check : " + Integer.toBinaryString(crc) + " -
			// " + Integer.toBinaryString(temp));
			if (temp == crc) {
				return true;
			}
		}

		return false;
	}

	private static boolean checkSum(String signal, int startIndex, int endIndex, int crcStartIndex) {
		int sum = 0;
		if (signal.length() != 65) {
			System.err.println("length not right!" + signal.length());
		} else {
			int index = startIndex;
			while (index < endIndex) {
				int tmp = 0;
				for (int i = 0; i < 8; i++) {
					if ((index + i) < endIndex && signal.charAt(index + i) == '1') {
						tmp |= (1 << (7 - i));
					}
				}
				// System.out.println(Integer.toBinaryString(sum)+"
				// "+Integer.toBinaryString(tmp));
				sum += tmp;
				index += 8;
			}

			int crc = 0;
			for (int j = crcStartIndex; j < crcStartIndex + 8; j ++) {
				crc = crc << 1;
				if (signal.charAt(j) == '1') {
					crc = crc | 1;
				}
			}

			 System.out.println("check : " + Integer.toBinaryString(crc) + " - " + Integer.toBinaryString(sum));


			while (Integer.numberOfLeadingZeros(sum) < 24) {
				int a = sum & 0xFF;
				int b = sum >> 8;
				// System.out.println(Integer.toBinaryString(sum)+"
				// "+Integer.toBinaryString(a)+" "+Integer.toBinaryString(b)+"
				// "+Integer.numberOfLeadingZeros(sum));
				sum = a + b;
			}
			sum = ~sum & 0xFF;
			// System.out.println(Integer.toBinaryString(sum));
			// System.out.println(Integer.toBinaryString(~sum));
			// System.out.println(Integer.toBinaryString(~sum & 0xFFFF));
			
			if (sum == crc) {
				return true;
			}
		}

		return false;
	}

	private static boolean checkBcc(String signal, int startIndex, int endIndex, int crcStartIndex) {
		int xor = 0;
		if (signal.length() != 65) {
			System.err.println("length not right!" + signal.length());
		} else {
			int index = startIndex;
			while (index < endIndex) {
				int tmp = 0;
				for (int i = 0; i < 8; i++) {
					if ((index + i) < endIndex && signal.charAt(index + i) == '1') {
						tmp |= (1 << (7 - i));
					}
				}
				// System.out.println(Integer.toBinaryString(tmp)+"
				// "+Integer.toBinaryString(xor));
				xor = xor ^ tmp;
				index += 8;
			}
			// System.out.println(Integer.toBinaryString(~sum & 0xFFFF));

			int crc = 0;
			for (int j = crcStartIndex; j < crcStartIndex + 8; j++) {
				crc = crc << 1;
				if (signal.charAt(j) == '1') {
					crc = crc | 1;
				}
			}

			// System.out.println("check : " + Integer.toBinaryString(crc) + " -
			// " + Integer.toBinaryString(xor));
			if (xor == crc) {
				return true;
			}
		}

		return false;
	}

	private static String signalSimplify(String s) {
		StringBuffer newSignal = new StringBuffer();
		for (int i = 0; i < s.length(); i += 2) {
			newSignal.append(s.charAt(i));
		}
		return newSignal.toString();
	}

	private static void parseSignalFile(FileReader file) throws IOException {
		BufferedReader reader = new BufferedReader(file);
		String line = null;
		Map<String, List<String>> map = new HashMap<String, List<String>>();
		while ((line = reader.readLine()) != null) {
			if (line.startsWith("//")) {
				line = line.substring(2);
			}
			line = line.split(" ")[0];
			if (line.length() < 130) {
				continue;
			}
			line = signalSimplify(line);
			String check = line.substring(57);
			if (!map.containsKey(check)) {
				List<String> list = new ArrayList<String>();
				map.put(check, list);
			}
			map.get(check).add(line);
		}
		for (String key : map.keySet()) {
			if (map.get(key).size() <= 1) {
				continue;
			}
			System.out.print(key);
			int sum = 0;
			for (int i = 0; i < key.length(); i += 1) {
				sum *= 2;
				if (key.charAt(i) == '1') {
					sum += 1;
				}
			}
			System.out.println(" " + sum);
			for (String s : map.get(key)) {
				System.out.println(s);
			}
		}
		reader.close();
	}

	private static void analyse() {
		// checkBcc(
		// "11100110001000000100110101100010000000100000000000000010100000001",
		// 0, 49, 49);
		// for (int i = 0; i < 41; i ++) {
		//
		// int startIndex = i;
		//
		// for (int j = i + 32; j < 57; j ++) {
		// int endIndex = j;
		//
		// if (checkBcc(
		// "11100110001000000100110101100010000000100000000000000010100000001",
		// startIndex, endIndex, 49)
		//// && checkBcc(
		//// "11100110001000000100110101100010000000100000000000000010100000001",
		//// startIndex, endIndex, 49)
		// ) {
		// System.out.println(startIndex + " " + endIndex);
		// }
		// }
		// System.out.println("----------------------");
		// }

		List<Integer> hxs = new ArrayList<Integer>();// { 0xD5, 0x2F, 0x31,
		// 0xA7, 0x07, 0x39,
		// 0x49, 0x1D, 0x9B };
		//generateHxs(hxs, 4, 1, 1);
		//System.out.println(hxs.size());

		for (int i = 0; i < 5; i ++) {

			int startIndex = i;

			for (int j = 57; j < 58; j ++) {
				int endIndex = j;

				for (int k = 57; k <= 57; k ++) {
					//for (int hx : hxs) {
						if (checkSum(
								"11100110001000000100110101100010000000100000000000000010100000001",
								startIndex, endIndex, k)
//								&& checkCRC(
//										"11100111000011011100000010100101000000010000110111110011100000001",
//										startIndex, endIndex, k, hx)
								&& checkSum(
										"11100111000010011000000011101000100001011000010000000101011011010",
										startIndex, endIndex, k)) {
							System.out.println(startIndex + " " + endIndex);// + " " + Integer.toHexString(hx));
						}
					//}
				}
			}
			System.out.println("----------------------");
		}
	}

	private static void generateHxs(List<Integer> hxs, int size, int index, int tmpHx) {
		while (index <= 8 - size) {
			int temp = tmpHx | (1 << index);
			if (size <= 1) {
				hxs.add(temp);
				System.out.println(Integer.toBinaryString(temp));
			} else {
				int j = index + 1;
				int size1 = size - 1;
				generateHxs(hxs, size1, j, temp);
			}
			index++;
		}
	}

	public static void main(String[] args) {
//		 FileReader fileReader = null;
//		 try {
//		 fileReader = new FileReader("input.txt");
//		 parseSignalFile(fileReader);
//		 } catch (IOException e) {
//		 // TODO Auto-generated catch block
//		 e.printStackTrace();
//		 } finally {
//		 try {
//		 fileReader.close();
//		 } catch (IOException e) {
//		 // TODO Auto-generated catch block
//		 e.printStackTrace();
//		 }
//		 }

		analyse();
	}

}

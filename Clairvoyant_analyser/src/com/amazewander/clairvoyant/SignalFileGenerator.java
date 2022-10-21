package com.amazewander.clairvoyant;

import java.io.BufferedReader;
import java.io.FileNotFoundException;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.IOException;

public class SignalFileGenerator {

	private static void generation(FileWriter file, String signals) throws IOException {
		if (signals.startsWith("//")) {
			return;
		}
		String s = signals.split(" ")[0];
		String name = signals.split(" ")[1];
		file.write("void signal" + name + "(){\n");
		// file.write(" set9833(DISABLE_OUTPUT, 2);\n");
		// file.write(" set9833(ENABLE_OUTPUT, 2);\n");
		file.write("  startSignal();\n");
		char lastChar = '1';
		int i = 2;
		System.out.println(s);
		System.out.println(name);
		for (; i < s.length() - 3; i = i + 2) {
			if (lastChar != s.charAt(i)) {
				file.write("  change();\n");
				lastChar = s.charAt(i);
			} else {
				file.write("  noChange();\n");
			}
		}
		if (lastChar != s.charAt(i)) {
			file.write("  changeEndSignal();\n}\n\n");
			lastChar = s.charAt(i);
		} else {
			file.write("  endSignal();\n}\n\n");
		}
	}

	private static void parseSignalFile(FileReader file, FileWriter writer) throws IOException {
		BufferedReader reader = new BufferedReader(file);
		String line = null;
		while ((line = reader.readLine()) != null) {
			// generation(writer, line);
			// parseLevelToArray(line);
			parseLevelToDelayArray(line);
		}
		reader.close();

		StringBuffer sb = new StringBuffer();
		sb.append("static uint8 levelSignals[][] = {\n");
		for (String s : signals) {
			sb.append("\t").append(s).append(",\n");
		}
		sb.deleteCharAt(sb.length() - 2);
		sb.append("};");
		System.out.println(sb.toString());
	}

	private static String signals[] = new String[11];

	private static void parseLevelToArray(String line) {
		if (line.split(" ")[1].startsWith("Level_")) {
			String signal = line.split(" ")[0];
			int sum = 0;
			int delta = 1;
			StringBuffer sb = new StringBuffer();
			sb.append("{");
			for (int i = 0; i < signal.length(); i += 2) {
				if (i > 0 && i % 16 == 0) {
					sb.append("0x").append(Integer.toHexString(sum)).append(",");
					sum = 0;
					delta = 1;
				}
				if (signal.charAt(i) == '1') {
					sum += delta;
				}
				delta *= 2;
			}
			sb.append("0x").append(Integer.toHexString(sum)).append("}");
			int level = Integer.parseInt(line.split(" ")[1].split("_")[1]);
			System.out.println("static uint8 level" + level + "Signal[] = " + sb.toString());
			signals[level - 1] = sb.toString();
		}
	}

	private static void parseLevelToDelayArray(String line) {
		if (line.split(" ")[1].startsWith("Level_")) {
			String signal = line.split(" ")[0];
			char lastChar = '1';
			int count = 0;
			int delta = 136;
			StringBuffer sb = new StringBuffer();
			sb.append("{");
			for (int i = 0; i < signal.length(); i += 2) {
				if (signal.charAt(i) != lastChar) {
					sb.append(count * 4 + delta).append(", ");
					count = 0;
					if (delta > 0) {
						delta = -1;
					}
					lastChar = signal.charAt(i); 
				}
				count++;
			}
			sb.append(count * 4 + delta).append("}");
			int level = Integer.parseInt(line.split(" ")[1].split("_")[1]);
			System.out.println("static uint8 level" + level + "Signal[] = " + sb.toString()+", "+sb.toString().split(",").length);
			signals[level - 1] = sb.toString();
		}
	}

	public static void main(String[] args) {
		FileWriter fileWritter = null;
		FileReader fileReader = null;
		try {
			fileWritter = new FileWriter(
					"F:/Workspaces/bitbucket/Clairvoyant/Clairvoyant_arduino_derailleur/derailleur_nano/signal.ino",
					false);
			fileReader = new FileReader("input.txt");
			parseSignalFile(fileReader, fileWritter);
		} catch (IOException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		} finally {
			try {
				fileWritter.close();
				fileReader.close();
			} catch (IOException e) {
				// TODO Auto-generated catch block
				e.printStackTrace();
			}
		}
	}

}

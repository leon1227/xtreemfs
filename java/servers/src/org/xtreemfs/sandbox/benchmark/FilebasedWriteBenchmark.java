/*
 * Copyright (c) 2012-2013 by Jens V. Fischer, Zuse Institute Berlin
 *
 *
 * Licensed under the BSD License, see LICENSE file for details.
 *
 */

package org.xtreemfs.sandbox.benchmark;

import java.io.IOException;
import java.util.LinkedList;
import java.util.Random;

import org.xtreemfs.common.libxtreemfs.FileHandle;
import org.xtreemfs.common.libxtreemfs.Volume;
import org.xtreemfs.pbrpc.generatedinterfaces.GlobalTypes;

/**
 * Class implementing a filebased write benchmark.
 * 
 * @author jensvfischer
 */
class FilebasedWriteBenchmark extends FilebasedBenchmark {

    private LinkedList<String> filenames;

    FilebasedWriteBenchmark(Volume volume, Config config) throws Exception {
        super(volume, config);
        filenames = new LinkedList<String>();
    }

    @Override
    void prepareBenchmark() throws Exception {
    }

    /* Called within the benchmark method. Performs the actual reading of data from the volume. */
    @Override
    long performIO(byte[] data, long numberOfBlocks) throws IOException {

        // long numberOfFiles = convertTo4KiBBlocks(numberOfBlocks);
        long numberOfFiles = config.randomSizeInBytes / 4096;
        long byteCounter = 0;
        Random random = new Random();

        int flags = GlobalTypes.SYSTEM_V_FCNTL.SYSTEM_V_FCNTL_H_O_CREAT.getNumber()
                | GlobalTypes.SYSTEM_V_FCNTL.SYSTEM_V_FCNTL_H_O_TRUNC.getNumber()
                | GlobalTypes.SYSTEM_V_FCNTL.SYSTEM_V_FCNTL_H_O_RDWR.getNumber();

        for (long j = 0; j < numberOfFiles; j++) {
            FileHandle fileHandle = volume.openFile(config.userCredentials, BENCHMARK_FILENAME + j, flags, 511);
            this.filenames.add(BENCHMARK_FILENAME + j);
            random.nextBytes(data);
            byteCounter += fileHandle.write(config.userCredentials, data, randomIOFilesize, 0);
            fileHandle.close();
        }
        return byteCounter;
    }

    @Override
    void finalizeBenchmark() throws Exception {
        VolumeManager.getInstance().setRandomFilelistForVolume(volume, filenames);
        VolumeManager.getInstance().addCreatedFiles(volume, filenames);
    }

}

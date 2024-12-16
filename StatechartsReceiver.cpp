/*
 * Context Object: ctx
 *
 * Description:
 * - `ctx` is an instance of the `ReceiverY` class, managing the receiver's state and interactions during
 *   the YMODEM protocol. It inherits common functionality from the `PeerY` base class.
 * - Provides methods and variables for session control, synchronization, block validation, and communication.
 *
 * Key Variables in `ctx`:
 * - `goodBlk` (bool): Indicates if the last block received is valid.
 * - `goodBlk1st` (bool): True if the current block is the first valid copy of a new block.
 * - `syncLoss` (bool): True if a fatal synchronization loss has occurred.
 * - `closeProb` (int): Indicates file closure status:
 *      0 = no problem, -1 = file not yet closed, positive = error code.
 * - `errCnt` (int): Counts errors or retries (e.g., repeated blocks, timeouts, excessive NAKs).
 * - `NCGbyte` (uint8_t): The initial control byte sent to synchronize with the sender, typically `'C'`.
 * - `KbCan` (bool): True if a keyboard cancel command (`KB_C`) has been issued.
 * - `anotherFile` (bool): True if another file exists in the batch transfer.
 * - `transferringFileD` (int): Descriptor for the file currently being received (-1 if no file is open).
 * - `result` (std::string): Logs the result or status of the session (e.g., "Done", "Cancelled").
 *
 * Key Methods in `ctx`:
 * - **Session Management:**
 *   - `cans()`: Sends multiple CAN characters to cancel the session.
 *   - `clearCan()`: Clears consecutive CAN characters from the peer.
 * - **Block Validation:**
 *   - `getRestBlk()`: Reads and validates the remaining bytes of a block after SOH.
 *   - `writeChunk()`: Writes valid block data to disk.
 * - **Communication:**
 *   - `sendByte(uint8_t)`: Sends a single byte to the peer.
 * - **Timeout Management:**
 *   - `tm(int tmSeconds)`: Sets an absolute timeout.
 *   - `tmPush(int tmSeconds)`: Temporarily adjusts the timeout.
 *   - `tmPop()`: Restores the previous timeout.
 *   - `tmRed(int secsToReduce)`: Reduces the current timeout.
 * - **File Management:**
 *   - `checkForAnotherFile()`: Checks if another file exists for transfer (triggered at the end of a file).
 * - **Buffer Management:**
 *   - `purge()`: Clears the input buffer.
 */
/*
 * Sends multiple CAN characters to notify the peer of session cancellation.
 *
 * Key Features:
 * - Sends `CAN_LEN` copies of the CAN character.
 * - Notifies the peer about cancellation clearly and explicitly.
 *
 * Purpose:
 * - Used in error cases, synchronization loss, or session termination.
 *
 * Context Variables:
 * - `ctx.KbCan`: Indicates if a keyboard cancellation (`KB_C`) has been issued.
 */
void cans();

/*
 * Reads remaining characters of a data block after receiving SOH.
 *
 * Key Features:
 * - Attempts to read 132 characters to form a complete block.
 * - Validates the block using CRC and adjusts synchronization status.
 * - Calls `purge()` for handling excess data when necessary.
 *
 * Boolean Updates:
 * - `ctx.goodBlk`: True if the block is valid.
 * - `ctx.syncLoss`: True if synchronization is irrecoverable.
 * - `ctx.goodBlk1st`: True if it is the first valid copy of the block.
 *
 * Context Variables:
 * - `ctx.errCnt`: Reset to 0 if the block is valid.
 * - `ctx.anotherFile`: Checked after block validation to determine the next action.
 */
void getRestBlk();

/*
 * Writes a valid block’s data to disk.
 *
 * Key Features:
 * - Writes only if `ctx.goodBlk1st` is true, ensuring no duplicate writes.
 * - Handles received data efficiently for persistent storage.
 *
 * Context Variables:
 * - `ctx.goodBlk1st`: Ensures the block is written only once.
 */
void writeChunk();

/*
 * Clears consecutive CAN characters received from the peer.
 *
 * Key Features:
 * - Reads up to `CAN_LEN - 2` characters or until timeout.
 * - Stops if a non-CAN character is received.
 * - Sends a non-CAN character to the console if received.
 *
 * Context Variables:
 * - `ctx.KbCan`: Monitored to prioritize cancellation.
 */
void clearCan();

/*
 * Purges the input buffer to discard excess characters.
 *
 * Key Features:
 * - Continues reading until no characters are received for 1 second.
 * - Ensures a clean buffer state for subsequent operations.
 */
void purge();

/*
 * Sends a single byte to the peer across the communication medium.
 *
 * Key Features:
 * - Minimal operation to transmit data.
 * - Used extensively for control characters (ACK, NAK, EOT, etc.).
 *
 * Parameters:
 * - byte: The byte value to send.
 */
void sendByte(uint8_t byte);

/*
 * Sets an absolute timeout for the next operation.
 *
 * Key Features:
 * - Determines the timeout as `current time + tmSeconds`.
 * - Used for precise synchronization of operations.
 *
 * Parameters:
 * - tmSeconds: The timeout duration in seconds.
 */
void tm(int tmSeconds);

/*
 * Temporarily adjusts the timeout with a push and pop mechanism.
 *
 * Key Features:
 * - `tmPush`: Stores the current timeout and sets a temporary one.
 * - `tmPop`: Restores the previously stored timeout.
 */
void tmPush(int tmSeconds);
void tmPop();

/*
 * Reduces the absolute timeout by a specified duration.
 *
 * Key Features:
 * - Makes the timeout earlier, useful for dynamic adjustments.
 *
 * Parameters:
 * - secsToReduce: The number of seconds to reduce from the timeout.
 */
void tmRed(int secsToReduce);

/*
 * Checks if another file is ready for transfer.
 *
 * Key Features:
 * - Evaluates the `ctx.anotherFile` variable.
 * - Prepares for the next file if available or signals the end of the session.
 *
 * Context Variables:
 * - `ctx.anotherFile`: Determines if the next file exists in the transfer batch.
 * - `ctx.transferringFileD`: Used to manage file descriptors during transfer.
 */
void checkForAnotherFile();

/*
 * Receiver State Descriptions
 */

/*
 * Top-Level Receiver State: Receiver_TopLevel
 *
 * Description:
 * - Handles the initialization of the receiver's state machine and manages transitions
 *   to operational states (`NON_CAN` and `CAN`).
 *
 * Entry Actions:
 * - Sends an initial readiness byte (`ctx.NCGbyte`) to synchronize with the sender.
 * - Resets error-related counters (`ctx.errCnt`) and synchronization variables (`ctx.closeProb`, `ctx.KbCan`).
 * - Initiates a timeout (`ctx.tm(TM_SOH)`) to await the first control character.
 *
 * Exit Actions:
 * - No explicit exit actions, as transitions directly delegate control to substates.
 *
 * Transitions:
 * 1. `SER` (SOH or CAN):
 *    - If `c == CAN`: Transition to `CAN` state.
 *    - If `c == SOH`: Transition to `NON_CAN`.
 * 2. `KB_C`:
 *    - Prioritizes keyboard cancellation, setting `ctx.KbCan = true`.
 *    - Cancels the session gracefully and records the cancellation result.
 */

/*
 * State: NON_CAN
 *
 * Description:
 * - Represents the normal operational mode, handling data reception and block validation.
 * - Substates include handling individual stages of block validation (`FirstByteStat`, `DataCancelable`).
 *
 * Entry Actions:
 * - No explicit entry actions for this state.
 *
 * Exit Actions:
 * - Transitions to `CAN` if a CAN character is received (`SER` event).
 *
 * Transitions:
 * 1. `SER` (c == CAN):
 *    - Transition to `CAN` to handle cancellation.
 * 2. Substate transitions:
 *    - Delegates to substates like `FirstByteStat`, `DataCancelable` based on control characters.
 */

/*
 * State: FirstByteStat (Substate of NON_CAN)
 *
 * Description:
 * - Waits for the first byte of a block (SOH for status blocks) and initiates validation.
 *
 * Entry Actions:
 * - No explicit actions on entry.
 *
 * Exit Actions:
 * - None.
 *
 * Transitions:
 * 1. `SER` (c == EOT):
 *    - If `ctx.errCnt >= errB`: Cancels session by transitioning to `FinalState`.
 *    - If `ctx.errCnt < errB`: Sends `ACK` and awaits the next operation.
 * 2. `SER` (c == SOH):
 *    - Transitions to `CondlTransientStat` for block validation.
 */

/*
 * State: CondlTransientStat (Substate of NON_CAN)
 *
 * Description:
 * - Handles the validation of status blocks and determines next steps based on block quality.
 *
 * Entry Actions:
 * - Posts a `CONT` event to proceed automatically.
 *
 * Exit Actions:
 * - None.
 *
 * Transitions:
 * 1. `CONT`:
 *    - If `!ctx.goodBlk`: Sends `NAK`, increments `errCnt`, and transitions back to `FirstByteStat`.
 *    - If `ctx.goodBlk`: Checks for additional files and transitions to `CondTransientCheck`.
 *    - If synchronization is lost or excessive errors (`ctx.syncLoss || ctx.errCnt >= errB`): Cancels session.
 */

/*
 * State: CondTransientCheck (Substate of NON_CAN)
 *
 * Description:
 * - Temporary state to evaluate if there are more files to transfer or if the session should terminate.
 *
 * Entry Actions:
 * - Posts a `CONT` event immediately to proceed to the next state or action.
 *
 * Exit Actions:
 * - None.
 *
 * Transitions:
 * 1. `CONT`:
 *    - If `!ctx.anotherFile`: Sends `ACK`, resets the timeout, and transitions to `AreWeDone`.
 *    - If `ctx.anotherFile`: Prepares the next file for transfer by transitioning to `CondTransientOpen`.
 */

/*
 * State: CondTransientOpen (Substate of NON_CAN)
 *
 * Description:
 * - Handles the transition to the next file transfer if another file exists.
 *
 * Entry Actions:
 * - Posts a `CONT` event to initiate file opening or session cleanup.
 *
 * Exit Actions:
 * - None.
 *
 * Transitions:
 * 1. `CONT`:
 *    - If `ctx.transferringFileD != -1`: Sends `ACK` and readiness byte (`NCGbyte`), resets the timeout, and transitions to `FirstByteData`.
 *    - If `ctx.transferringFileD == -1`: Cancels the session with an "OpenError" result and transitions to `FinalState`.
 */

/*
 * State: CondTransientEOT (Substate of NON_CAN)
 *
 * Description:
 * - Handles the end-of-transfer process triggered by the EOT character.
 *
 * Entry Actions:
 * - Posts a `CONT` event to finalize the session or prepare for the next file.
 *
 * Exit Actions:
 * - None.
 *
 * Transitions:
 * 1. `CONT`:
 *    - If `!ctx.closeProb`: Sends `ACK` and readiness byte (`NCGbyte`), logs the session as "Done", resets error counters, and transitions to `FirstByteStat`.
 *    - If `ctx.closeProb`: Cancels the session due to a close error and transitions to `FinalState`.
 */

/*
 * State: AreWeDone (Substate of NON_CAN)
 *
 * Description:
 * - Evaluates whether the session should end or transition to the next operation.
 *
 * Entry Actions:
 * - No explicit entry actions.
 *
 * Exit Actions:
 * - None.
 *
 * Transitions:
 * 1. `TM` (timeout):
 *    - Logs the session result as "EndOfSession" and transitions to `FinalState`.
 * 2. `SER` (c == SOH):
 *    - Attempts to read the next block using `ctx.getRestBlk()`.
 *    - Transitions to `CondlTransientStat` for further evaluation.
 */

/*
 * State: CondTransientData (Substate of DataCancelable)
 *
 * Description:
 * - Manages block validation and determines the next step based on block integrity.
 *
 * Entry Actions:
 * - Resets the timeout (`ctx.tm(0)`).
 *
 * Exit Actions:
 * - None.
 *
 * Transitions:
 * 1. `TM`:
 *    - If `ctx.goodBlk`: Sends `ACK`, writes the block (`ctx.writeChunk()`), and transitions to `DataCancelable`.
 *    - If `!ctx.goodBlk`: Sends `NAK` and retries block reception.
 *    - If `ctx.syncLoss || ctx.errCnt >= errB`: Cancels the session due to excessive errors and transitions to `FinalState`.
 * 2. `SER`: Clears excess characters using `ctx.purge()`.
 * 3. `KB_C`: Cancels the session immediately, transitioning to `FinalState`.
 */

/*
 * State: CAN
 *
 * Description:
 * - Handles session cancellation initiated by the peer or due to critical errors.
 *
 * Entry Actions:
 * - Clears consecutive CAN characters using `ctx.clearCan()`.
 * - Closes any open files and sets the session result as "SndCancelled".
 *
 * Exit Actions:
 * - None.
 *
 * Transitions:
 * 1. `SER` (c != CAN):
 *    - Clears the input buffer (`ctx.purge()`).
 *    - Transitions back to `NON_CAN` if no further CAN characters are received.
 * 2. `KB_C`:
 *    - Handles keyboard cancellation while in this state.
 * 3. `TM` (timeout event):
 *    - Resets the timeout and transitions back to `NON_CAN`.
 * 4. `SER` (c == CAN):
 *    - Stays in the `CAN` state to handle repeated CAN characters.
 */




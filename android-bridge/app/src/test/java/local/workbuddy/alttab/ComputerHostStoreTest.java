package local.workbuddy.alttab;

import static org.junit.Assert.assertEquals;

import org.junit.Test;

public final class ComputerHostStoreTest {
    @Test
    public void saveTrimsHostAndEmptyValueClearsIt() {
        FakeStorage storage = new FakeStorage();
        ComputerHostStore store = new ComputerHostStore(storage);

        store.set(" 100.81.73.56 ");
        assertEquals("100.81.73.56", storage.value);
        assertEquals("100.81.73.56", store.get());

        store.set("   ");
        assertEquals("", storage.value);
        assertEquals("", store.get());
    }

    @Test
    public void missingStoredValueReadsAsEmpty() {
        FakeStorage storage = new FakeStorage();
        storage.value = null;

        assertEquals("", new ComputerHostStore(storage).get());
    }

    private static final class FakeStorage implements ComputerHostStore.Storage {
        private String value;

        @Override
        public String load() {
            return value;
        }

        @Override
        public void save(String host) {
            value = host;
        }
    }
}

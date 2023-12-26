import Head from "next/head";
import styles from "@/styles/Home.module.css";
import { useEffect, useState } from "react";

export default function Home() {
  const [desiredTemp, setDesiredTemp] = useState(0);
  const [status, setStatus] = useState({
    status: "BOOST_ON",
    heatmat: "ON",
    temperatures: { dough: 23, box: 32 },
  });

  useEffect(() => {
    const interval = setInterval(() => {
      fetch("http://proofuino.local/status")
        .then((response) => response.json())
        .then((data) => setStatus(data))
        .catch((error) => console.error(error));
    }, 2000);

    return () => clearInterval(interval);
  }, []);

  return (
    <>
      <Head>
        <title>Proofuino</title>
      </Head>
      <main>
        <div style={{ paddingBottom: "20px" }}>Proofuino</div>
        <form
          onSubmit={(event) => {
            //send post request to /temperature
            fetch("http://proofuino.local/temperature", {
              method: "POST",
              headers: {
                "Content-Type": "application/json",
              },
              body: JSON.stringify({ desiredDoughTemperature: desiredTemp }),
            })
              .then((response) => response.json())
              .then((data) => {
                console.log("Success:", data);
              })
              .catch((error) => {
                console.error("Error:", error);
              });
            //prevent full page reload which is the default behavior
            event?.preventDefault();
          }}
        >
          <label>Desired Dough Temperature</label>
          <input
            type="text"
            value={desiredTemp}
            onChange={(e) => {
              const input = Number(e.target.value);
              setDesiredTemp(input);
            }}
          />
          <input type="submit" value="Submit" />
        </form>
        <div className={styles.parent}>
          <div className={styles.child}>Status</div>
          <div className={styles.child}>{status.status}</div>
        </div>
        <div className={styles.parent}>
          <div className={styles.child}>Heatmat</div>
          <div className={styles.child}>{status.heatmat}</div>
        </div>
        <div className={styles.parent}>
          <div className={styles.child}>Dough Temperature</div>
          <div className={styles.child}>{status.temperatures.dough}</div>
        </div>
        <div className={styles.parent}>
          <div className={styles.child}>Box Temperature</div>
          <div className={styles.child}>{status.temperatures.box}</div>
        </div>
      </main>
    </>
  );
}

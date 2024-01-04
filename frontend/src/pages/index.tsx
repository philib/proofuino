import Head from "next/head";
import styles from "@/styles/Home.module.css";
import React, { ReactElement, ReactNode, useEffect, useState } from "react";
import { Button, FormGroup, Switch, TextField } from "@mui/material";

const isDev = process.env.NODE_ENV === "development";

type ProofuinoActiveState = "ON" | "OFF";

interface Status {
  state: string;
  config: {
    targetTemperature: number;
  };
  sensors: {
    relay: "ON" | "OFF";
    temperatures: {
      dough: number;
      box: number;
    };
  };
}
export default function Home() {
  const [targetTemp, setTargetTemp] = useState<number>(0);
  const [status, setStatus] = useState<Status | null>(null);

  const fetchStatus = () => {
    if (isDev) {
      setStatus({
        state: "MOCKED",
        config: {
          targetTemperature: 999,
        },
        sensors: {
          relay: "ON",
          temperatures: {
            dough: 123,
            box: 123,
          },
        },
      });
      return;
    }
    fetch(`http://${window.location.host}/status`)
      .then((response) => response.json())
      .then((data) => {
        setStatus(data);
      })
      .catch((error) => console.error(error));
  };

  useEffect(() => {
    fetchStatus();
    if (isDev) return;
    const interval = setInterval(fetchStatus, 10000);
    return () => clearInterval(interval);
  }, []);

  const applyTargetTemperature = (newTargetTemp: number) => {
    fetch(`http://${window.location.host}/config`, {
      method: "POST",
      headers: {
        "Content-Type": "application/json",
      },
      body: JSON.stringify({
        targetTemperature: newTargetTemp,
      }),
    }),
      //prevent full page reload which is the default behavior
      event?.preventDefault();
  };

  const applyProofuinoActiveState = (state: ProofuinoActiveState) => {
    if (isDev && status != null) {
      setStatus({
        ...status,
        state,
      });
      return;
    }
    fetch(`http://${window.location.host}/${state === "ON" ? "on" : "off"}`, {
      method: "POST",
    });
  };

  interface ChildWithState {
    status: Status;
  }
  const WithProofuinoStatus: React.FunctionComponent<{
    children: React.FunctionComponent<ChildWithState>;
  }> = ({ children }) => <>{status == null ? "Loading..." : children}</>;

  return (
    <>
      <Head>
        <title>Proofuino</title>
      </Head>
      <main className={styles.spaContainer}>
        <div className={styles.header}>
          Proofuino
          {status != null && (
            <Switch
              defaultChecked
              checked={status.state != "PAUSED"}
              onChange={(e) => {
                let status: ProofuinoActiveState = e.target.checked
                  ? "ON"
                  : "OFF";
                applyProofuinoActiveState(status);
              }}
            />
          )}
        </div>
        <div className={styles.infoGrid}>
          <div className={styles.infoBox}>
            <div className={styles.infoTitle}>Sensors</div>
            <div className={styles.infoContent}>
              <WithProofuinoStatus>
                {({ status }) => (
                  <>
                    <text>Dough: {status.sensors.temperatures.dough}°C</text>
                    <br />
                    <text>Box: {status.sensors.temperatures.box}°C</text>
                    <br />
                    <text>Relay: {status.sensors.relay}</text>
                  </>
                )}
              </WithProofuinoStatus>
            </div>
          </div>
          {status != null && status.state != "PAUSED" && (
            <>
              <div className={styles.infoBox}>
                <div className={styles.infoTitle}>Status</div>

                <div className={styles.infoContent}>
                  {status == null ? (
                    "Loading..."
                  ) : (
                    <>
                      <text>State: {status.state}</text>
                      <br />
                      <text>
                        Target Dough Temperature:{" "}
                        {status.config.targetTemperature}°C
                      </text>
                    </>
                  )}
                </div>
              </div>
              <div className={styles.infoBox}>
                <div className={styles.infoTitle}>Controls</div>
                <form
                  style={{ marginTop: 10 }}
                  onSubmit={(e) => {
                    applyTargetTemperature(targetTemp);
                    e.preventDefault();
                  }}
                >
                  <FormGroup>
                    <TextField
                      id="outlined-basic"
                      label="Target Dough Temperature"
                      type="number"
                      variant="outlined"
                      onChange={(e) => {
                        let n = Number(e.target.value);
                        if (Number.isSafeInteger(n)) {
                          setTargetTemp(Number(e.target.value));
                        }
                      }}
                    />
                    <Button
                      variant="contained"
                      type="submit"
                      value={"Set"}
                      style={{ marginTop: 5 }}
                    >
                      Set
                    </Button>
                  </FormGroup>
                </form>
              </div>
            </>
          )}
        </div>
      </main>
    </>
  );
}
